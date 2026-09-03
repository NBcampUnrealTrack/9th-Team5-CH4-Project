#include "DRSnowRenderUpdateBatcher.h"

#include "DRSnowVoxelContainmentEvaluator.h"
#include "Engine/World.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelWorld.h"

namespace
{
constexpr float RenderUpdateDelaySeconds = 0.1f;
}

FDRSnowRenderUpdateBatcher::FDRSnowRenderUpdateBatcher(
	FDRSnowVoxelContainmentEvaluator& InContainmentEvaluator)
	: ContainmentEvaluator(InContainmentEvaluator)
{
}

FDRSnowRenderUpdateBatcher::~FDRSnowRenderUpdateBatcher()
{
	Reset();
	World.Reset();
}

void FDRSnowRenderUpdateBatcher::Initialize(UWorld* InWorld)
{
	if (World == InWorld)
	{
		return;
	}

	Reset();
	World = InWorld;
}

void FDRSnowRenderUpdateBatcher::Enqueue(
	AVoxelWorld* VoxelWorld,
	const FVoxelIntBox& Bounds,
	const EDRSnowRenderUpdateType UpdateType)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || !Bounds.IsValid())
	{
		return;
	}

	FPendingUpdate* PendingUpdate = PendingUpdates.FindByPredicate(
		[VoxelWorld](const FPendingUpdate& Entry)
		{
			return Entry.VoxelWorld == VoxelWorld;
		});
	if (!PendingUpdate)
	{
		PendingUpdate = &PendingUpdates.AddDefaulted_GetRef();
		PendingUpdate->VoxelWorld = VoxelWorld;
	}

	FPendingBounds MergedEntry;
	MergedEntry.Bounds = Bounds;
	MergedEntry.bEvaluateVoxelContainment =
		UpdateType == EDRSnowRenderUpdateType::Geometry;
	for (int32 Index = 0; Index < PendingUpdate->Bounds.Num();)
	{
		if (!MergedEntry.Bounds.Intersect(PendingUpdate->Bounds[Index].Bounds))
		{
			++Index;
			continue;
		}

		MergedEntry.Bounds =
			MergedEntry.Bounds + PendingUpdate->Bounds[Index].Bounds;
		MergedEntry.bEvaluateVoxelContainment |=
			PendingUpdate->Bounds[Index].bEvaluateVoxelContainment;
		PendingUpdate->Bounds.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		Index = 0;
	}
	PendingUpdate->Bounds.Add(MoveTemp(MergedEntry));

	UWorld* LocalWorld = World.Get();
	if (IsValid(LocalWorld) && !LocalWorld->GetTimerManager().IsTimerActive(FlushTimerHandle))
	{
		LocalWorld->GetTimerManager().SetTimer(
			FlushTimerHandle,
			FTimerDelegate::CreateRaw(this, &FDRSnowRenderUpdateBatcher::Flush),
			RenderUpdateDelaySeconds,
			false);
	}
}

void FDRSnowRenderUpdateBatcher::Reset()
{
	if (UWorld* LocalWorld = World.Get())
	{
		LocalWorld->GetTimerManager().ClearTimer(FlushTimerHandle);
	}
	PendingUpdates.Reset();
}

void FDRSnowRenderUpdateBatcher::Flush()
{
	for (FPendingUpdate& PendingUpdate : PendingUpdates)
	{
		AVoxelWorld* VoxelWorld = PendingUpdate.VoxelWorld.Get();
		if (IsValid(VoxelWorld) && VoxelWorld->IsCreated() && !PendingUpdate.Bounds.IsEmpty())
		{
			TArray<FVoxelIntBox> BoundsToUpdate;
			BoundsToUpdate.Reserve(PendingUpdate.Bounds.Num());
			for (const FPendingBounds& PendingBounds : PendingUpdate.Bounds)
			{
				// Directional 편집의 밀려나기가 끝난 뒤, collision 갱신 직전에 검사한다.
				if (PendingBounds.bEvaluateVoxelContainment)
				{
					ContainmentEvaluator.EvaluateCharactersInEditedBounds(
						*VoxelWorld,
						PendingBounds.Bounds);
				}
				BoundsToUpdate.Add(PendingBounds.Bounds);
			}
			VoxelWorld->GetLODManager().UpdateBounds(BoundsToUpdate);
		}
	}
	PendingUpdates.Reset();
}
