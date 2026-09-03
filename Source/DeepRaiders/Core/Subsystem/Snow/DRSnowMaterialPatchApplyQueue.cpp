#include "DRSnowMaterialPatchApplyQueue.h"

#include "DRSnowSurfaceEditor.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"

FDRSnowMaterialPatchApplyQueue::FDRSnowMaterialPatchApplyQueue(
	FDRSnowSurfaceEditor& InSurfaceEditor)
	: SurfaceEditor(InSurfaceEditor)
{
}

void FDRSnowMaterialPatchApplyQueue::Enqueue(
	AVoxelWorld* VoxelWorld,
	FDRSnowMaterialPatch MaterialPatch)
{
	if (!IsValid(VoxelWorld) || MaterialPatch.IsEmpty())
	{
		return;
	}

	FPendingPatch PendingPatch;
	PendingPatch.VoxelWorld = VoxelWorld;
	PendingPatch.Patch = MoveTemp(MaterialPatch);
	PendingPatch.StateGeneration = CurrentStateGeneration;
	PendingPatches.Enqueue(MoveTemp(PendingPatch));
	ProcessNext();
}

void FDRSnowMaterialPatchApplyQueue::Reset()
{
	++CurrentStateGeneration;

	FPendingPatch PendingPatch;
	while (PendingPatches.Dequeue(PendingPatch))
	{
	}
	// 이미 실행 중인 Voxel Plugin 작업은 취소할 수 없다. 완료 콜백에서 generation을
	// 확인해 오래된 렌더 갱신을 버리고, 그 뒤 새 generation의 요청을 처리한다.
}

void FDRSnowMaterialPatchApplyQueue::ProcessNext()
{
	if (bPatchInProgress)
	{
		return;
	}

	FPendingPatch PendingPatch;
	while (PendingPatches.Dequeue(PendingPatch))
	{
		AVoxelWorld* VoxelWorld = PendingPatch.VoxelWorld.Get();
		if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
			PendingPatch.StateGeneration != CurrentStateGeneration)
		{
			continue;
		}

		bPatchInProgress = true;
		const TWeakPtr<FDRSnowMaterialPatchApplyQueue> WeakQueue = AsShared();
		const TWeakObjectPtr<AVoxelWorld> WeakVoxelWorld(VoxelWorld);
		const int32 PatchGeneration = PendingPatch.StateGeneration;
		const bool bStarted = SurfaceEditor.ApplySnowMaterialPatchAsync(
			VoxelWorld,
			MoveTemp(PendingPatch.Patch),
			[WeakQueue, WeakVoxelWorld, PatchGeneration](
				const bool bApplied,
				TArray<FVoxelIntBox>&& EditedChunkBounds)
			{
				if (const TSharedPtr<FDRSnowMaterialPatchApplyQueue> Queue = WeakQueue.Pin())
				{
					Queue->HandlePatchCompleted(
						WeakVoxelWorld,
						PatchGeneration,
						bApplied,
						MoveTemp(EditedChunkBounds));
				}
			});

		if (!bStarted)
		{
			bPatchInProgress = false;
			continue;
		}
		return;
	}
}

void FDRSnowMaterialPatchApplyQueue::HandlePatchCompleted(
	const TWeakObjectPtr<AVoxelWorld> VoxelWorld,
	const int32 PatchGeneration,
	const bool bApplied,
	TArray<FVoxelIntBox>&& EditedChunkBounds)
{
	AVoxelWorld* ValidVoxelWorld = VoxelWorld.Get();
	if (bApplied && PatchGeneration == CurrentStateGeneration &&
		IsValid(ValidVoxelWorld) && ValidVoxelWorld->IsCreated())
	{
		for (const FVoxelIntBox& EditedChunkBoundsEntry : EditedChunkBounds)
		{
			UVoxelBlueprintLibrary::UpdateBounds(
				ValidVoxelWorld,
				EditedChunkBoundsEntry.Extend(1));
		}
	}

	bPatchInProgress = false;
	ProcessNext();
}
