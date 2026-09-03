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
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		return ExecuteDirectionalAdd(World, Request);
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
	const float AuthoritativeAmount)
{
	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		return Execute(World, Request);
	}

	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return {};
	}

	return ExecuteDirectionalAdd(World, Request, AuthoritativeAmount);
}

FDRSnowAddResult FDRSnowAddPipeline::ExecuteDirectionalAdd(
	UWorld* World,
	const FDRSnowSurfaceAddRequest& Request,
	const float AuthoritativeAmount)
{
	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return Result;
	}

	const float AppliedAmount = AuthoritativeAmount > 0.f
		? AuthoritativeAmount
		: EditResult.AppliedAmount;
	CommitAddedSurfaceEdit(World, Request, EditResult, AppliedAmount);
	Result.AddedAmount = AppliedAmount;
	return Result;
}

void FDRSnowAddPipeline::CommitAddedSurfaceEdit(
	UWorld* World,
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
			VolumeAmount >= 0.f ? VolumeAmount : EditResult.AppliedAmount);
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
