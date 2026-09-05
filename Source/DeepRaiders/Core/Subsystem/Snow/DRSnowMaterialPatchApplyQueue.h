#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "DeepRaiders/Snow/DRSnowReplicationTypes.h"
#include "VoxelIntBox.h"

class AVoxelWorld;
class FDRSnowSurfaceEditor;

// 네트워크로 받은 MaterialPatch를 수신 순서대로 하나씩 비동기 적용한다.
// 각 패치가 끝나면 편집 청크를 즉시 렌더 갱신한 뒤 다음 작업을 시작한다.
class DEEPRAIDERS_API FDRSnowMaterialPatchApplyQueue
	: public TSharedFromThis<FDRSnowMaterialPatchApplyQueue>
{
public:
	explicit FDRSnowMaterialPatchApplyQueue(FDRSnowSurfaceEditor& InSurfaceEditor);

	void Enqueue(
		AVoxelWorld* VoxelWorld,
		FDRSnowMaterialPatch MaterialPatch);
	void Reset();
	bool IsIdle() const { return !bPatchInProgress && PendingPatches.IsEmpty(); }

private:
	struct FPendingPatch
	{
		TWeakObjectPtr<AVoxelWorld> VoxelWorld;
		FDRSnowMaterialPatch Patch;
		int32 StateGeneration = 0;
	};

	void ProcessNext();
	void HandlePatchCompleted(
		TWeakObjectPtr<AVoxelWorld> VoxelWorld,
		int32 PatchGeneration,
		bool bApplied,
		TArray<FVoxelIntBox>&& EditedChunkBounds);

	FDRSnowSurfaceEditor& SurfaceEditor;
	TQueue<FPendingPatch> PendingPatches;
	int32 CurrentStateGeneration = 0;
	bool bPatchInProgress = false;
};
