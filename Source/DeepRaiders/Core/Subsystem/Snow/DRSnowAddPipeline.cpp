#include "DRSnowAddPipeline.h"

#include "DRSnowOwnershipStore.h"
#include "DRSnowSurfaceEditor.h"
#include "DRSnowVolumeStore.h"
#include "DRSnowVoxelContainmentEvaluator.h"
#include "DeepRaiders/Snow/DRSnowMaterialMapping.h"
#include "Engine/World.h"
#include "VoxelWorld.h"

FDRSnowAddPipeline::FDRSnowAddPipeline(
	FDRSnowSurfaceEditor& InSurfaceEditor,
	FDRSnowOwnershipStore& InOwnershipStore,
	FDRSnowVolumeStore& InVolumeStore,
	FDRSnowVoxelContainmentEvaluator& InContainmentEvaluator)
	: SurfaceEditor(InSurfaceEditor)
	, OwnershipStore(InOwnershipStore)
	, VolumeStore(InVolumeStore)
	, ContainmentEvaluator(InContainmentEvaluator)
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

	// Ownership은 서버의 MaterialIndex 원본이다. 클라이언트는 authoritative patch만 적용한다.
	if (IsValid(World) && World->GetNetMode() != NM_Client)
	{
		OwnershipStore.RecordAddedVoxels(
			VoxelWorld,
			EditResult.ModifiedValues,
			DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
	}
	if (EditResult.bUseModifiedValuesForVolume)
	{
		AddVolumeFromModifiedValues(
			*VoxelWorld,
			Request,
			EditResult.ModifiedValues,
			VolumeAmount.IsSet() ? VolumeAmount.GetValue() : EditResult.AppliedAmount);
	}

	ContainmentEvaluator.EvaluateSurfaceEdit(EditResult);
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
