#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "DeepRaiders/Snow/DRSnowReplicationTypes.h"
#include "VoxelIntBox.h"

class AVoxelWorld;
class FDRSnowRenderUpdateBatcher;
class FDRSnowSurfaceEditor;

// 네트워크로 받은 MaterialPatch를 수신 순서대로 하나씩 비동기 적용한다.
// 서로 다른 패치의 Voxel 쓰기가 겹치지 않도록 완료 콜백에서 다음 작업을 시작한다.
class DEEPRAIDERS_API FDRSnowMaterialPatchApplyQueue
	: public TSharedFromThis<FDRSnowMaterialPatchApplyQueue>
{
public:
	void Initialize(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowRenderUpdateBatcher& InRenderUpdateBatcher,
		int32 InitialStateGeneration);

	void Enqueue(
		AVoxelWorld* VoxelWorld,
		FDRSnowMaterialPatch MaterialPatch,
		int32 StateGeneration);
	void Reset(int32 NewStateGeneration);

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

	FDRSnowSurfaceEditor* SurfaceEditor = nullptr;
	FDRSnowRenderUpdateBatcher* RenderUpdateBatcher = nullptr;
	TQueue<FPendingPatch> PendingPatches;
	int32 CurrentStateGeneration = 0;
	bool bPatchInProgress = false;
};
