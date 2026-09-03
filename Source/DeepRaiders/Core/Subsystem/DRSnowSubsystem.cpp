#include "DRSnowSubsystem.h"

#include "DeepRaiders/Snow/DRSnowMaterialMapping.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelWorld.h"

UDRSnowSubsystem::UDRSnowSubsystem()
{
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(VolumeStore);
}

bool UDRSnowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

FDRSnowAddResult UDRSnowSubsystem::AddSnow(
	const FDRSnowSurfaceAddRequest& Request,
	TFunction<void(float)> DirectionalCompletion)
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
		FDRSnowPendingDirectionalAdd PendingAdd;
		PendingAdd.Request = Request;
		PendingAdd.Completion = MoveTemp(DirectionalCompletion);
		DirectionalAddQueue.Enqueue(MoveTemp(PendingAdd));
		ProcessNextDirectionalAdd();
		// 실제 성공 여부는 Completion에서 전달한다. 반환값은 큐 접수 여부다.
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

		ApplyAddedSurfaceEdit(Request, EditResult);
		return VolumeStore.AddSnow(Request);
	}

	Result = VolumeStore.AddSnow(Request);
	if (Result.AddedAmount > 0.f)
	{
		const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
		ApplyAddedSurfaceEdit(Request, EditResult);
	}
	return Result;
}

FDRSnowAddResult UDRSnowSubsystem::ApplyReplicatedSnowAdd(
	const FDRSnowSurfaceAddRequest& Request,
	const float AppliedAmount)
{
	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		return AddSnow(Request);
	}

	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;
	SurfaceEditor.SetWorld(GetWorld());
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return Result;
	}

	const float AuthoritativeAmount = AppliedAmount > 0.f
		? AppliedAmount
		: EditResult.AppliedAmount;
	ApplyAddedSurfaceEdit(Request, EditResult, AuthoritativeAmount);
	QueueRenderUpdate(EditResult.VoxelWorld.Get(), EditResult.EditedBounds);
	Result.AddedAmount = AuthoritativeAmount;
	return Result;
}

void UDRSnowSubsystem::ProcessNextDirectionalAdd()
{
	if (bDirectionalAddInProgress)
	{
		return;
	}

	FDRSnowPendingDirectionalAdd PendingAdd;
	if (!DirectionalAddQueue.Dequeue(PendingAdd))
	{
		return;
	}
	const FDRSnowSurfaceAddRequest Request = PendingAdd.Request;

	bDirectionalAddInProgress = true;
	SurfaceEditor.SetWorld(GetWorld());
	const TWeakObjectPtr<UDRSnowSubsystem> WeakThis(this);
	const int32 RequestGeneration = SnowStateGeneration;
	const bool bStarted = SurfaceEditor.AddDirectionalSnowAtAreaAsync(
		Request,
		[WeakThis, Request, RequestGeneration, Completion = PendingAdd.Completion](FDRSnowSurfaceEditResult&& EditResult) mutable
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
				if (Completion)
				{
					Completion(EditResult.AppliedAmount);
				}
				SnowSubsystem->bDirectionalAddInProgress = false;
				SnowSubsystem->ProcessNextDirectionalAdd();
			}
		});

	if (!bStarted)
	{
		if (PendingAdd.Completion)
		{
			PendingAdd.Completion(0.f);
		}
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
			VoxelWorld->GetLODManager().UpdateBounds(PendingUpdate.Bounds);
		}
	}
	PendingRenderUpdates.Reset();
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnow(
	const FDRSnowSurfaceRemoveRequest& Request,
	FDRSnowMaterialPatch* OutMaterialPatch)
{
	if (OutMaterialPatch)
	{
		*OutMaterialPatch = FDRSnowMaterialPatch();
	}

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

	FDRSnowResolvedMaterialEdit ResolvedEdit;
	if (SurfaceEditor.ResolveSnowMaterialsAtArea(
		Request,
		EditResult,
		OwnershipStore,
		VolumeStore,
		ResolvedEdit))
	{
		if (OutMaterialPatch)
		{
			*OutMaterialPatch = ResolvedEdit.MaterialPatch;
		}
		SurfaceEditor.ApplyResolvedSnowMaterials(ResolvedEdit);
	}
	return Result;
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnowWithAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	FDRSnowMaterialPatch* OutMaterialPatch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Pipeline_Total);
	if (OutMaterialPatch)
	{
		*OutMaterialPatch = FDRSnowMaterialPatch();
	}

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

	FDRSnowResolvedMaterialEdit ResolvedEdit;
	if (SurfaceEditor.ResolveSnowMaterialsAtModifiedVoxels(
		Request,
		EditResult,
		OwnershipStore,
		VolumeStore,
		ResolvedEdit))
	{
		if (OutMaterialPatch)
		{
			*OutMaterialPatch = ResolvedEdit.MaterialPatch;
		}
		SurfaceEditor.ApplyResolvedSnowMaterials(ResolvedEdit);
	}
	return Result;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
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

	// Geometry는 로컬 VoxelWorld에서 재현하지만 Volume 감소 상한은 서버 확정량을 사용한다.
	ApplyRemovedSurfaceEdit(Request, EditResult, AppliedAmount);
	if (AuthoritativeMaterialPatch)
	{
		SurfaceEditor.ApplySnowMaterialPatch(
			EditResult.VoxelWorld.Get(),
			*AuthoritativeMaterialPatch);
		return true;
	}

	// 패치 플래그가 없는 구형 record만 로컬 Store 기반 repaint로 fallback 한다.
	return RepaintSnowMaterialsAtArea(Request, EditResult);
}

bool UDRSnowSubsystem::ApplyReplicatedSnowAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
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
	ApplyRemovedSurfaceEdit(Request, EditResult, AppliedAmount);
	if (AuthoritativeMaterialPatch)
	{
		SurfaceEditor.ApplySnowMaterialPatch(
			EditResult.VoxelWorld.Get(),
			*AuthoritativeMaterialPatch);
		return true;
	}

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
	FDRSnowPendingDirectionalAdd PendingAdd;
	while (DirectionalAddQueue.Dequeue(PendingAdd))
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

bool UDRSnowSubsystem::ApplyCheckpoint(
	FName Name,
	const TArray<uint8>& Voxel,
	const TArray<uint8>& Volume)
{
	// 중도 난입 클라이언트에는 Ownership 원본을 복원하지 않는다.
	OwnershipStore.Reset();
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->ApplyCheckpoint(Name, Voxel, Volume);
}

void UDRSnowSubsystem::ApplyAddedSurfaceEdit(
	const FDRSnowSurfaceAddRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const float VolumeAmount)
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!IsValid(VoxelWorld))
	{
		return;
	}

	// Ownership은 서버의 MaterialIndex 원본이다. 클라이언트는 authoritative patch만 적용한다.
	if (UWorld* World = GetWorld(); IsValid(World) && World->GetNetMode() != NM_Client)
	{
		OwnershipStore.RecordAddedVoxels(
			VoxelWorld,
			EditResult.ModifiedValues,
			DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
	}
	if (!EditResult.bUseModifiedValuesForVolume)
	{
		return;
	}

	AddVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		VolumeAmount >= 0.f ? VolumeAmount : EditResult.AppliedAmount);
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

	if (UWorld* World = GetWorld(); IsValid(World) && World->GetNetMode() != NM_Client)
	{
		OwnershipStore.RemoveClearedVoxels(VoxelWorld, EditResult.ModifiedValues);
	}
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
