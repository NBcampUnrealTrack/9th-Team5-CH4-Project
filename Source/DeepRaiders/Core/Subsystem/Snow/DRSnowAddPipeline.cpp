#include "DRSnowAddPipeline.h"

#include "DRSnowOwnershipStore.h"
#include "DRSnowRenderUpdateBatcher.h"
#include "DRSnowSurfaceEditor.h"
#include "DRSnowVolumeStore.h"
#include "DeepRaiders/Snow/DRSnowMaterialMapping.h"
#include "Engine/World.h"
#include "VoxelWorld.h"

FDRSnowAddPipeline::FDRSnowAddPipeline(
	FDRSnowSurfaceEditor& InSurfaceEditor,
	FDRSnowOwnershipStore& InOwnershipStore,
	FDRSnowVolumeStore& InVolumeStore,
	FDRSnowRenderUpdateBatcher& InRenderUpdateBatcher)
	: SurfaceEditor(InSurfaceEditor)
	, OwnershipStore(InOwnershipStore)
	, VolumeStore(InVolumeStore)
	, RenderUpdateBatcher(InRenderUpdateBatcher)
{
}

void FDRSnowAddPipeline::Initialize(
	UWorld* InWorld,
	const int32 InitialStateGeneration)
{
	World = InWorld;
	CurrentStateGeneration = InitialStateGeneration;
	SurfaceEditor.SetWorld(InWorld);
}

FDRSnowAddResult FDRSnowAddPipeline::Execute(
	const FDRSnowSurfaceAddRequest& Request,
	TFunction<void(float)> DirectionalCompletion)
{
	FDRSnowAddResult Result;
	UWorld* LocalWorld = World.Get();
	SurfaceEditor.SetWorld(LocalWorld);
	if (!IsValid(LocalWorld))
	{
		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		FPendingDirectionalAdd PendingAdd;
		PendingAdd.Request = Request;
		PendingAdd.Completion = MoveTemp(DirectionalCompletion);
		PendingAdd.StateGeneration = CurrentStateGeneration;
		PendingDirectionalAdds.Enqueue(MoveTemp(PendingAdd));
		ProcessNextDirectionalAdd();

		Result.TeamId = Request.Context.TeamId;
		// Directional 경로의 실제 성공량은 Completion으로 전달하고, 반환값은 큐 접수량이다.
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

FDRSnowAddResult FDRSnowAddPipeline::Replay(
	const FDRSnowSurfaceAddRequest& Request,
	const float AuthoritativeAmount)
{
	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		return Execute(Request);
	}

	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;
	SurfaceEditor.SetWorld(World.Get());
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return Result;
	}

	const float AppliedAmount = AuthoritativeAmount > 0.f
		? AuthoritativeAmount
		: EditResult.AppliedAmount;
	ApplyAddedSurfaceEdit(Request, EditResult, AppliedAmount);
	RenderUpdateBatcher.Enqueue(EditResult.VoxelWorld.Get(), EditResult.EditedBounds);
	Result.AddedAmount = AppliedAmount;
	return Result;
}

void FDRSnowAddPipeline::Reset(const int32 NewStateGeneration)
{
	CurrentStateGeneration = NewStateGeneration;

	FPendingDirectionalAdd PendingAdd;
	while (PendingDirectionalAdds.Dequeue(PendingAdd))
	{
	}
	// 실행 중인 작업은 완료 콜백에서 generation을 확인한 뒤 폐기한다.
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
		if (PendingAdd.StateGeneration != CurrentStateGeneration)
		{
			continue;
		}

		bDirectionalAddInProgress = true;
		SurfaceEditor.SetWorld(World.Get());
		const FDRSnowSurfaceAddRequest Request = PendingAdd.Request;
		const int32 RequestGeneration = PendingAdd.StateGeneration;
		TFunction<void(float)> Completion = MoveTemp(PendingAdd.Completion);
		const TWeakPtr<FDRSnowAddPipeline> WeakPipeline = AsShared();
		const bool bStarted = SurfaceEditor.AddDirectionalSnowAtAreaAsync(
			Request,
			[WeakPipeline,
			 Request,
			 RequestGeneration,
			 Completion](FDRSnowSurfaceEditResult&& EditResult) mutable
			{
				if (const TSharedPtr<FDRSnowAddPipeline> Pipeline = WeakPipeline.Pin())
				{
					Pipeline->HandleDirectionalAddCompleted(
						Request,
						RequestGeneration,
						MoveTemp(Completion),
						MoveTemp(EditResult));
				}
			});

		if (!bStarted)
		{
			if (Completion)
			{
				Completion(0.f);
			}
			bDirectionalAddInProgress = false;
			continue;
		}
		return;
	}
}

void FDRSnowAddPipeline::HandleDirectionalAddCompleted(
	const FDRSnowSurfaceAddRequest& Request,
	const int32 RequestGeneration,
	TFunction<void(float)> Completion,
	FDRSnowSurfaceEditResult&& EditResult)
{
	if (RequestGeneration == CurrentStateGeneration)
	{
		ApplyAddedSurfaceEdit(Request, EditResult);
		RenderUpdateBatcher.Enqueue(EditResult.VoxelWorld.Get(), EditResult.EditedBounds);
		if (Completion)
		{
			Completion(EditResult.AppliedAmount);
		}
	}

	bDirectionalAddInProgress = false;
	ProcessNextDirectionalAdd();
}

void FDRSnowAddPipeline::ApplyAddedSurfaceEdit(
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
	if (UWorld* LocalWorld = World.Get(); IsValid(LocalWorld) && LocalWorld->GetNetMode() != NM_Client)
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
