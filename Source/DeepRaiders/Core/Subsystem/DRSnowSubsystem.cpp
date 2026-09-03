#include "DRSnowSubsystem.h"

#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelWorld.h"

namespace
{
	bool EvaluateCharactersInEditedBounds(
		AVoxelWorld& VoxelWorld,
		const FVoxelIntBox& EditedBounds)
	{
		if (!EditedBounds.IsValid())
		{
			return false;
		}

		FBox EditedWorldBounds(ForceInit);
		const FIntVector Min = EditedBounds.Min;
		const FIntVector Max = EditedBounds.Max;
		for (int32 X = 0; X < 2; ++X)
		{
			for (int32 Y = 0; Y < 2; ++Y)
			{
				for (int32 Z = 0; Z < 2; ++Z)
				{
					EditedWorldBounds += VoxelWorld.LocalToGlobal(FIntVector(
						X == 0 ? Min.X : Max.X,
						Y == 0 ? Min.Y : Max.Y,
						Z == 0 ? Min.Z : Max.Z));
				}
			}
		}
		EditedWorldBounds = EditedWorldBounds.ExpandBy(VoxelWorld.VoxelSize);

		bool bEvaluatedAnyCharacter = false;
		if (EditedWorldBounds.IsValid)
		{
			for (TActorIterator<ACharacter> It(VoxelWorld.GetWorld()); It; ++It)
			{
				ACharacter* Character = *It;
				const UCapsuleComponent* Capsule =
					IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
				if (!IsValid(Capsule) ||
					!EditedWorldBounds.Intersect(Capsule->Bounds.GetBox()))
				{
					continue;
				}

				if (UDRCharacterMovementComponent* Movement =
					Cast<UDRCharacterMovementComponent>(Character->GetMovementComponent()))
				{
					Movement->EvaluateVoxelContainment(&VoxelWorld);
					bEvaluatedAnyCharacter = true;
				}
			}
		}
		return bEvaluatedAnyCharacter;
	}

	void EvaluateAffectedVoxelContainment(
		const FDRSnowSurfaceAddRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult)
	{
		AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
		if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
		{
			return;
		}

		const bool bEvaluatedAnyCharacter =
			EvaluateCharactersInEditedBounds(*VoxelWorld, EditResult.EditedBounds);

		// 일부 동기 편집 경로는 EditedBounds를 제공하지 않으므로 요청자를 fallback으로 검사한다.
		if (!bEvaluatedAnyCharacter)
		{
			if (APawn* InstigatorPawn = Request.Context.InstigatorPawn.Get())
			{
				if (UDRCharacterMovementComponent* Movement =
					Cast<UDRCharacterMovementComponent>(
						InstigatorPawn->GetMovementComponent()))
				{
					Movement->EvaluateVoxelContainment(VoxelWorld);
				}
			}
		}
	}
}

UDRSnowSubsystem::UDRSnowSubsystem()
{
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(
		VolumeStore,
		OwnershipStore);
}

bool UDRSnowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

FDRSnowAddResult UDRSnowSubsystem::AddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}
	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		Result.TeamId = Request.Context.TeamId;
		DirectionalAddQueue.Enqueue(Request);
		ProcessNextDirectionalAdd();
		// 호출자는 비동기 작업 접수를 기준으로 네트워크 작업을 등록한다.
		Result.AddedAmount = Request.Amount;
		return Result;
	}
	if (Request.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool)
	{
		// 눈벽은 실제 복셀 부피가 먼저 만들어져야 원본 Volume도 같은 결과로 기록한다.
		const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
		if (EditResult.AppliedAmount <= 0.f)
		{
			return Result;
		}

		EvaluateAffectedVoxelContainment(Request, EditResult);
		return VolumeStore.AddSnow(Request);
	}

	Result = VolumeStore.AddSnow(Request);
	if (Result.AddedAmount > 0.f)
	{
		const FDRSnowSurfaceEditResult EditResult =
			SurfaceEditor.AddSnowAtArea(Request);
		EvaluateAffectedVoxelContainment(Request, EditResult);
	}
	return Result;
}

void UDRSnowSubsystem::ProcessNextDirectionalAdd()
{
	if (bDirectionalAddInProgress)
	{
		return;
	}

	FDRSnowSurfaceAddRequest Request;
	if (!DirectionalAddQueue.Dequeue(Request))
	{
		return;
	}

	bDirectionalAddInProgress = true;
	SurfaceEditor.SetWorld(GetWorld());
	const TWeakObjectPtr<UDRSnowSubsystem> WeakThis(this);
	const int32 RequestGeneration = SnowStateGeneration;
	const bool bStarted = SurfaceEditor.AddDirectionalSnowAtAreaAsync(
		Request,
		[WeakThis, Request, RequestGeneration](FDRSnowSurfaceEditResult&& EditResult)
		{
			if (UDRSnowSubsystem* SnowSubsystem = WeakThis.Get())
			{
				if (SnowSubsystem->SnowStateGeneration != RequestGeneration)
				{
					return;
				}

				// 완료된 실제 변경 voxel만 원본 데이터에 반영한 뒤 다음 요청을 시작한다.
				SnowSubsystem->ApplyAddedSurfaceEdit(Request, EditResult);
				SnowSubsystem->QueueRenderUpdate(EditResult.VoxelWorld.Get(), EditResult.EditedBounds);
				SnowSubsystem->bDirectionalAddInProgress = false;
				SnowSubsystem->ProcessNextDirectionalAdd();
			}
		});

	if (!bStarted)
	{
		bDirectionalAddInProgress = false;
		ProcessNextDirectionalAdd();
	}
}

void UDRSnowSubsystem::QueueRenderUpdate(AVoxelWorld* VoxelWorld, const FVoxelIntBox& Bounds)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || !Bounds.IsValid())
	{
		return;
	}

	FDRSnowPendingRenderUpdate* PendingUpdate = PendingRenderUpdates.FindByPredicate(
		[VoxelWorld](const FDRSnowPendingRenderUpdate& Entry)
		{
			return Entry.VoxelWorld == VoxelWorld;
		});
	if (!PendingUpdate)
	{
		PendingUpdate = &PendingRenderUpdates.AddDefaulted_GetRef();
		PendingUpdate->VoxelWorld = VoxelWorld;
	}

	// 같은 월드에서 겹치는 편집 영역은 한 번만 렌더 갱신한다.
	FVoxelIntBox MergedBounds = Bounds;
	for (int32 Index = 0; Index < PendingUpdate->Bounds.Num();)
	{
		if (!MergedBounds.Intersect(PendingUpdate->Bounds[Index]))
		{
			++Index;
			continue;
		}

		MergedBounds = MergedBounds + PendingUpdate->Bounds[Index];
		PendingUpdate->Bounds.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		Index = 0;
	}
	PendingUpdate->Bounds.Add(MergedBounds);

	UWorld* World = GetWorld();
	if (IsValid(World) && !World->GetTimerManager().IsTimerActive(RenderUpdateTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			RenderUpdateTimerHandle,
			this,
			&ThisClass::FlushRenderUpdates,
			0.1f,
			false);
	}
}

void UDRSnowSubsystem::FlushRenderUpdates()
{
	for (FDRSnowPendingRenderUpdate& PendingUpdate : PendingRenderUpdates)
	{
		AVoxelWorld* VoxelWorld = PendingUpdate.VoxelWorld.Get();
		if (IsValid(VoxelWorld) && VoxelWorld->IsCreated() && !PendingUpdate.Bounds.IsEmpty())
		{
			// 발사 직후의 밀려나기가 끝난 위치에서, collision 갱신 바로 전에 검사한다.
			for (const FVoxelIntBox& Bounds : PendingUpdate.Bounds)
			{
				EvaluateCharactersInEditedBounds(*VoxelWorld, Bounds);
			}
			VoxelWorld->GetLODManager().UpdateBounds(PendingUpdate.Bounds);
		}
	}
	PendingRenderUpdates.Reset();
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.RemoveSnowAtArea(Request);
	Result.RemovedAmount = EditResult.AppliedAmount;
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}
	ApplyRemovedSurfaceEdit(Request, EditResult, Result.RemovedAmount);
	RepaintSnowMaterialsAtArea(Request, EditResult);
	return Result;
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnowWithAbsorbTool(const FDRSnowSurfaceRemoveRequest& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Pipeline_Total);
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.RemoveSnowWithAbsorbTool(Request);
	Result.RemovedAmount = EditResult.AppliedAmount;
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}
	ApplyRemovedSurfaceEdit(Request, EditResult, Result.RemovedAmount);
	SurfaceEditor.RepaintSnowMaterialsAtModifiedVoxels(
		Request,
		EditResult,
		OwnershipStore,
		VolumeStore);
	return Result;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	float AppliedAmount)
{
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World) || AppliedAmount <= 0.f)
	{
		return false;
	}

	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.RemoveSnowAtArea(Request);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return false;
	}

	ApplyRemovedSurfaceEdit(Request, EditResult, EditResult.AppliedAmount);

	return RepaintSnowMaterialsAtArea(Request, EditResult);
}

bool UDRSnowSubsystem::ApplyReplicatedSnowAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	float AppliedAmount)
{
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World) || AppliedAmount <= 0.f)
	{
		return false;
	}
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.RemoveSnowWithAbsorbTool(Request);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return false;
	}
	ApplyRemovedSurfaceEdit(Request, EditResult, EditResult.AppliedAmount);
	SurfaceEditor.RepaintSnowMaterialsAtModifiedVoxels(
		Request,
		EditResult,
		OwnershipStore,
		VolumeStore);
	return true;
}

bool UDRSnowSubsystem::RepaintSnowMaterialsAtArea(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult)
{
	SurfaceEditor.SetWorld(GetWorld());
	return SurfaceEditor.RepaintSnowMaterialsAtArea(
		Request,
		EditResult,
		OwnershipStore,
		VolumeStore);
}

int32 UDRSnowSubsystem::GetDominantTeamAtLocation(FVector Location) const
{
	return VolumeStore.GetDominantTeamAtLocation(Location);
}

FDRSnowControlRatio UDRSnowSubsystem::QuerySnowInBounds(const FBox& Bounds) const
{
	return VolumeStore.QuerySnowInBounds(Bounds);
}

FDRJoinSnapshotSizeReport UDRSnowSubsystem::MeasureCompressedSnapshotSize(AVoxelWorld* Target, bool bLog)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->MeasureCompressedSnapshotSize(Target, bLog);
}

bool UDRSnowSubsystem::CreateCheckpoint(int32 Sequence, AVoxelWorld* Target)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->CreateCheckpoint(Sequence, Target);
}

bool UDRSnowSubsystem::GetLatestCheckpointOperationSequence(int32& OutOperationSequence)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetLatestCheckpointOperationSequence(OutOperationSequence);
}

void UDRSnowSubsystem::ResetCheckpoints()
{
	if (SnapshotSerializer)
	{
		SnapshotSerializer->ResetCheckpoints();
	}
}

void UDRSnowSubsystem::ResetSnowState()
{
	++SnowStateGeneration;
	ResetCheckpoints();
	VolumeStore.Reset();
	OwnershipStore.Reset();
	PendingRenderUpdates.Reset();
	bDirectionalAddInProgress = false;
	FDRSnowSurfaceAddRequest PendingRequest;
	while (DirectionalAddQueue.Dequeue(PendingRequest))
	{
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RenderUpdateTimerHandle);
	}
}

bool UDRSnowSubsystem::GetLatestCheckpoint(FDRSnowJoinCheckpoint& Out)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetLatestCheckpoint(Out);
}

bool UDRSnowSubsystem::GetCheckpoint(int32 Id, FDRSnowJoinCheckpoint& Out)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetCheckpoint(Id, Out);
}

bool UDRSnowSubsystem::ApplyCheckpoint(FName Name, const TArray<uint8>& Voxel, const TArray<uint8>& Volume, const TArray<uint8>& Ownership)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->ApplyCheckpoint(Name, Voxel, Volume, Ownership);
}

void UDRSnowSubsystem::ApplyAddedSurfaceEdit(
	const FDRSnowSurfaceAddRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult)
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!EditResult.bUseModifiedValuesForVolume || !IsValid(VoxelWorld))
	{
		return;
	}

	OwnershipStore.RecordAddedVoxels(
		VoxelWorld,
		EditResult.ModifiedValues,
		Request.Context.TeamId);
	AddVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		EditResult.AppliedAmount);
}

void UDRSnowSubsystem::ApplyRemovedSurfaceEdit(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	float VolumeAmount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_ApplyRemovedSurfaceEdit);
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!EditResult.bUseModifiedValuesForVolume || !IsValid(VoxelWorld))
	{
		return;
	}

	OwnershipStore.RemoveClearedVoxels(VoxelWorld, EditResult.ModifiedValues);
	RemoveVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		VolumeAmount);
}

void UDRSnowSubsystem::AddVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceAddRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	float MaxAddedAmount)
{
	float AddedAmount = 0.f;
	const float VoxelRadius = FMath::Max(1.f, VoxelWorld.VoxelSize * 0.75f);
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		const float RemainingAmount = MaxAddedAmount - AddedAmount;
		if (RemainingAmount <= 0.f)
		{
			break;
		}

		if (ModifiedValue.NewValue >= ModifiedValue.OldValue)
		{
			continue;
		}

		FDRSnowSurfaceAddRequest CellRequest = Request;
		CellRequest.WorldLocation = VoxelWorld.LocalToGlobal(ModifiedValue.Position);
		CellRequest.Radius = VoxelRadius;
		CellRequest.Amount = FMath::Min(
			RemainingAmount,
			FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue));
		AddedAmount += VolumeStore.AddSnow(CellRequest).AddedAmount;
	}
}

void UDRSnowSubsystem::RemoveVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceRemoveRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	float MaxRemovedAmount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_RemoveVolumeFromModifiedValues);
	float RemovedAmount = 0.f;
	const float VoxelRadius = FMath::Max(1.f, VoxelWorld.VoxelSize * 0.75f);
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		const float RemainingAmount = MaxRemovedAmount - RemovedAmount;
		if (RemainingAmount <= 0.f)
		{
			break;
		}

		if (ModifiedValue.NewValue <= ModifiedValue.OldValue)
		{
			continue;
		}

		FDRSnowSurfaceRemoveRequest CellRequest = Request;
		CellRequest.WorldLocation = VoxelWorld.LocalToGlobal(ModifiedValue.Position);
		CellRequest.Radius = VoxelRadius;
		CellRequest.RequestedAmount = FMath::Min(
			RemainingAmount,
			FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue));
		RemovedAmount += VolumeStore.RemoveSnow(CellRequest).RemovedAmount;
	}
}
