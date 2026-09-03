#include "DRSnowRenderUpdateBatcher.h"

#include "Engine/World.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelWorld.h"

namespace
{
constexpr float RenderUpdateDelaySeconds = 0.1f;
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
	const FVoxelIntBox& Bounds)
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
			VoxelWorld->GetLODManager().UpdateBounds(PendingUpdate.Bounds);
		}
	}
	PendingUpdates.Reset();
}
