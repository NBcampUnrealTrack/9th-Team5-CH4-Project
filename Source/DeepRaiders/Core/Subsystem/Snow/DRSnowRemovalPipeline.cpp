#include "DRSnowRemovalPipeline.h"

#include "DRSnowOwnershipStore.h"
#include "DRSnowSurfaceEditor.h"
#include "DRSnowVolumeStore.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelWorld.h"

FDRSnowRemovalPipeline::FDRSnowRemovalPipeline(
	FDRSnowSurfaceEditor& InSurfaceEditor,
	FDRSnowOwnershipStore& InOwnershipStore,
	FDRSnowVolumeStore& InVolumeStore)
	: SurfaceEditor(InSurfaceEditor)
	, OwnershipStore(InOwnershipStore)
	, VolumeStore(InVolumeStore)
{
}

FDRSnowRemovalExecutionResult FDRSnowRemovalPipeline::Execute(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath,
	const bool bBuildMaterialPatch)
{
	FDRSnowRemovalExecutionResult Result;
	Result.RemoveResult.TeamId = Request.Context.TeamId;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}

	const FDRSnowSurfaceEditResult EditResult = RemoveSurface(Request, RemovalPath);
	Result.RemoveResult.RemovedAmount = EditResult.AppliedAmount;
	if (Result.RemoveResult.RemovedAmount <= 0.f)
	{
		return Result;
	}

	ApplyRemovedSurfaceEdit(
		World,
		Request,
		EditResult,
		Result.RemoveResult.RemovedAmount);

	FDRSnowResolvedMaterialEdit ResolvedEdit;
	if (ResolveMaterials(Request, EditResult, RemovalPath, ResolvedEdit))
	{
		SurfaceEditor.ApplyResolvedSnowMaterials(
			ResolvedEdit,
			bBuildMaterialPatch ? &Result.MaterialPatch : nullptr);
	}
	return Result;
}

FDRSnowRemovalReplayResult FDRSnowRemovalPipeline::Replay(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AuthoritativeAmount,
	const EDRSnowRemovalPath RemovalPath)
{
	FDRSnowRemovalReplayResult Result;
	SurfaceEditor.SetWorld(World);
	AVoxelWorld* VoxelWorld = Request.TargetVoxelWorld.Get();
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		AuthoritativeAmount <= 0.f || Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return Result;
	}

	const FDRSnowSurfaceEditResult EditResult = RemoveSurface(Request, RemovalPath);
	if (EditResult.AppliedAmount > 0.f)
	{
		ApplyRemovedSurfaceEdit(World, Request, EditResult, AuthoritativeAmount);
	}

	// 로컬에서 이미 비어 있어도 서버 재질 패치를 적용하고 다음 작업으로 진행한다.
	Result.VoxelWorld = VoxelWorld;
	Result.bApplied = true;
	return Result;
}

FDRSnowSurfaceEditResult FDRSnowRemovalPipeline::RemoveSurface(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath) const
{
	return RemovalPath == EDRSnowRemovalPath::Absorb
		? SurfaceEditor.RemoveSnowWithAbsorbTool(Request)
		: SurfaceEditor.RemoveSnowAtArea(Request);
}

bool FDRSnowRemovalPipeline::ResolveMaterials(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const EDRSnowRemovalPath RemovalPath,
	FDRSnowResolvedMaterialEdit& OutResolvedEdit) const
{
	return RemovalPath == EDRSnowRemovalPath::Absorb
		? SurfaceEditor.ResolveSnowMaterialsAtModifiedVoxels(
			Request,
			EditResult,
			OwnershipStore,
			VolumeStore,
			OutResolvedEdit)
		: SurfaceEditor.ResolveSnowMaterialsAtArea(
			Request,
			EditResult,
			OwnershipStore,
			VolumeStore,
			OutResolvedEdit);
}

void FDRSnowRemovalPipeline::ApplyRemovedSurfaceEdit(
	UWorld* World,
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

	if (IsValid(World) && World->GetNetMode() != NM_Client)
	{
		OwnershipStore.RemoveClearedVoxels(VoxelWorld, EditResult.ModifiedValues);
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
