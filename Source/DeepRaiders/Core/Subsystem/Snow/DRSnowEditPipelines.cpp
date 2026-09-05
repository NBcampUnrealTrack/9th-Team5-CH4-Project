#include "DRSnowEditPipelines.h"
#include "DRSnowSurfaceEditor.h"
#include "DRSnowVolumeStore.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "Engine/World.h"
#include "VoxelWorld.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "DeepRaiders/Player/Components/DRVoxelContainmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

namespace
{
void EvaluateCharactersInEditedBounds(
	AVoxelWorld& VoxelWorld,
	const FVoxelIntBox& EditedBounds)
{
	UWorld* World = VoxelWorld.GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client || !EditedBounds.IsValid())
	{
		return;
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

	if (!EditedWorldBounds.IsValid)
	{
		return;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		const UCapsuleComponent* Capsule =
			IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
		if (!IsValid(Capsule) ||
			!EditedWorldBounds.Intersect(Capsule->Bounds.GetBox()))
		{
			continue;
		}

		if (UDRVoxelContainmentComponent* Containment =
			Character->FindComponentByClass<UDRVoxelContainmentComponent>())
		{
			Containment->EvaluateVoxelContainment(&VoxelWorld);
		}
	}
}

void EvaluateSurfaceEdit(
	const FDRSnowSurfaceEditResult& EditResult)
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (EditResult.AppliedAmount <= 0.f ||
		!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!EditResult.EditedBounds.IsValid())
	{
		return;
	}

	EvaluateCharactersInEditedBounds(*VoxelWorld, EditResult.EditedBounds);
}

}

FDRSnowAddPipeline::FDRSnowAddPipeline(
	FDRSnowSurfaceEditor& InSurfaceEditor,
	FDRSnowVolumeStore& InVolumeStore)
	: SurfaceEditor(InSurfaceEditor)
	, VolumeStore(InVolumeStore)
{
}

FDRSnowAddResult FDRSnowAddPipeline::Execute(
	UWorld* World,
	const FDRSnowSurfaceAddRequest& Request,
	TFunction<void(float)> DirectionalCompletion)
{
	FDRSnowAddResult Result;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		FPendingDirectionalAdd PendingAdd;
		PendingAdd.World = World;
		PendingAdd.Request = Request;
		PendingAdd.Completion = MoveTemp(DirectionalCompletion);
		PendingAdd.StateGeneration = CurrentStateGeneration;
		PendingDirectionalAdds.Enqueue(MoveTemp(PendingAdd));
		ProcessNextDirectionalAdd();

		Result.TeamId = Request.Context.TeamId;
		// 실제 적용량은 비동기 완료 콜백으로 전달하며, 반환값은 큐 접수량이다.
		Result.AddedAmount = Request.Amount;
		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool)
	{
		const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
		if (EditResult.AppliedAmount <= 0.f)
		{
			return Result;
		}

		CommitAddedSurfaceEdit(World, Request, EditResult);
		return VolumeStore.AddSnow(Request);
	}

	Result = VolumeStore.AddSnow(Request);
	if (Result.AddedAmount > 0.f)
	{
		const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
		CommitAddedSurfaceEdit(World, Request, EditResult);
	}
	return Result;
}

FDRSnowAddResult FDRSnowAddPipeline::Replay(
	UWorld* World,
	const FDRSnowSurfaceAddRequest& Request,
	const float AuthoritativeAmount,
	TFunction<void(float)> DirectionalCompletion)
{
	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		return Execute(World, Request);
	}

	SurfaceEditor.SetWorld(World);
	if (!IsValid(World) || AuthoritativeAmount <= 0.f)
	{
		return {};
	}

	FPendingDirectionalAdd PendingAdd;
	PendingAdd.World = World;
	PendingAdd.Request = Request;
	PendingAdd.AuthoritativeAmount = AuthoritativeAmount;
	PendingAdd.Completion = MoveTemp(DirectionalCompletion);
	PendingAdd.StateGeneration = CurrentStateGeneration;
	PendingDirectionalAdds.Enqueue(MoveTemp(PendingAdd));
	ProcessNextDirectionalAdd();

	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;
	// 실제 복셀 편집은 비동기 큐에서 처리하며 반환값은 권위 작업의 접수량이다.
	Result.AddedAmount = AuthoritativeAmount;
	return Result;
}

void FDRSnowAddPipeline::Reset(const int32 NewStateGeneration)
{
	CurrentStateGeneration = NewStateGeneration;

	FPendingDirectionalAdd PendingAdd;
	while (PendingDirectionalAdds.Dequeue(PendingAdd))
	{
		if (PendingAdd.Completion)
		{
			PendingAdd.Completion(0.f);
		}
	}
	// 실행 중인 작업은 완료 콜백에서 generation을 확인한 뒤 결과를 폐기한다.
}

void FDRSnowAddPipeline::ProcessNextDirectionalAdd()
{
	if (bDirectionalAddInProgress)
	{
		return;
	}

	FPendingDirectionalAdd PendingAdd;
	while (PendingDirectionalAdds.Dequeue(PendingAdd))
	{
		UWorld* World = PendingAdd.World.Get();
		if (PendingAdd.StateGeneration != CurrentStateGeneration || !IsValid(World))
		{
			if (PendingAdd.Completion)
			{
				PendingAdd.Completion(0.f);
			}
			continue;
		}

		bDirectionalAddInProgress = true;
		SurfaceEditor.SetWorld(World);
		const FDRSnowSurfaceAddRequest Request = PendingAdd.Request;
		const TOptional<float> AuthoritativeAmount = PendingAdd.AuthoritativeAmount;
		const int32 RequestGeneration = PendingAdd.StateGeneration;
		TFunction<void(float)> Completion = MoveTemp(PendingAdd.Completion);
		const TFunction<void(float)> FailureCompletion = Completion;
		const TWeakPtr<FDRSnowAddPipeline> WeakPipeline = AsShared();
		const bool bStarted = SurfaceEditor.AddDirectionalSnowAtAreaAsync(
			Request,
			[WeakPipeline,
				WeakWorld = PendingAdd.World,
				Request,
				AuthoritativeAmount,
				RequestGeneration,
				Completion = MoveTemp(Completion)](FDRSnowSurfaceEditResult&& EditResult) mutable
			{
				if (const TSharedPtr<FDRSnowAddPipeline> Pipeline = WeakPipeline.Pin())
				{
					Pipeline->HandleDirectionalAddCompleted(
						WeakWorld,
						Request,
						AuthoritativeAmount,
						RequestGeneration,
						MoveTemp(Completion),
						MoveTemp(EditResult));
				}
			});

		if (!bStarted)
		{
			if (FailureCompletion)
			{
				FailureCompletion(0.f);
			}
			bDirectionalAddInProgress = false;
			continue;
		}
		return;
	}
}

void FDRSnowAddPipeline::HandleDirectionalAddCompleted(
	const TWeakObjectPtr<UWorld> World,
	const FDRSnowSurfaceAddRequest& Request,
	const TOptional<float> AuthoritativeAmount,
	const int32 RequestGeneration,
	TFunction<void(float)> Completion,
	FDRSnowSurfaceEditResult&& EditResult)
{
	float CompletedAmount = 0.f;
	if (RequestGeneration == CurrentStateGeneration && IsValid(World.Get()))
	{
		if (EditResult.AppliedAmount > 0.f)
		{
			CompletedAmount = AuthoritativeAmount.IsSet()
				? AuthoritativeAmount.GetValue()
				: EditResult.AppliedAmount;
			CommitAddedSurfaceEdit(World.Get(), Request, EditResult, CompletedAmount);
		}
	}

	if (Completion)
	{
		Completion(CompletedAmount);
	}
	bDirectionalAddInProgress = false;
	ProcessNextDirectionalAdd();
}

void FDRSnowAddPipeline::CommitAddedSurfaceEdit(
	UWorld* World,
	const FDRSnowSurfaceAddRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const TOptional<float> VolumeAmount)
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!IsValid(VoxelWorld))
	{
		return;
	}

	// 서버와 클라이언트 모두 새로 추가한 내부 복셀의 재질까지 확정한다.
	SurfaceEditor.FillAddedSnowMaterials(EditResult, Request.Context.TeamId);

	if (EditResult.bUseModifiedValuesForVolume)
	{
		AddVolumeFromModifiedValues(
			*VoxelWorld,
			Request,
			EditResult.ModifiedValues,
			VolumeAmount.IsSet() ? VolumeAmount.GetValue() : EditResult.AppliedAmount);
	}

	EvaluateSurfaceEdit(EditResult);
}

void FDRSnowAddPipeline::AddVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceAddRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	const float MaxAddedAmount)
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

FDRSnowRemovalPipeline::FDRSnowRemovalPipeline(
	FDRSnowSurfaceEditor& InSurfaceEditor,
	FDRSnowVolumeStore& InVolumeStore)
	: SurfaceEditor(InSurfaceEditor)
	, VolumeStore(InVolumeStore)
{
}

FDRSnowRemoveResult FDRSnowRemovalPipeline::Execute(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}

	const FDRSnowSurfaceEditResult EditResult = RemoveSurface(Request, RemovalPath);
	Result.RemovedAmount = EditResult.AppliedAmount;
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}

	ApplyRemovedSurfaceEdit(
		Request,
		EditResult,
		Result.RemovedAmount);

	return Result;
}

bool FDRSnowRemovalPipeline::Replay(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AuthoritativeAmount,
	const EDRSnowRemovalPath RemovalPath)
{
	SurfaceEditor.SetWorld(World);
	AVoxelWorld* VoxelWorld = Request.TargetVoxelWorld.Get();
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		AuthoritativeAmount <= 0.f || Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return false;
	}

	const FDRSnowSurfaceEditResult EditResult = RemoveSurface(Request, RemovalPath);
	if (EditResult.AppliedAmount > 0.f)
	{
		ApplyRemovedSurfaceEdit(Request, EditResult, AuthoritativeAmount);
	}

	// 로컬에서 이미 비어 있어도 다음 권위 작업으로 진행한다.
	return true;
}

FDRSnowSurfaceEditResult FDRSnowRemovalPipeline::RemoveSurface(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath) const
{
	return RemovalPath == EDRSnowRemovalPath::Absorb
		? SurfaceEditor.RemoveSnowWithAbsorbTool(Request)
		: SurfaceEditor.RemoveSnowAtArea(Request);
}

void FDRSnowRemovalPipeline::ApplyRemovedSurfaceEdit(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const float VolumeAmount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_ApplyRemovedSurfaceEdit);
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!EditResult.bUseModifiedValuesForVolume || !IsValid(VoxelWorld))
	{
		return;
	}

	RemoveVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		VolumeAmount);
}

void FDRSnowRemovalPipeline::RemoveVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceRemoveRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	const float MaxRemovedAmount)
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
