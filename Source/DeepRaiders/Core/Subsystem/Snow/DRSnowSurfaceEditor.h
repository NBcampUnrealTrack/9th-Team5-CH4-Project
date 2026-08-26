#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"

class AVoxelWorld;
class FDRSnowOwnershipStore;
class FDRSnowVolumeStore;

// Voxel 표현 편집의 실제 결과다. 원본 Store 갱신은 Subsystem이 이 결과를 사용해 처리한다.
struct FDRSnowSurfaceEditResult
{
	// 요청량이 아니라 Voxel 값이 실제로 변한 양이다.
	float AppliedAmount = 0.f;
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;
	FVoxelIntBox EditedBounds;
	// DirectionalSurfaceTool처럼 원본 Store가 실제 변경 위치를 따라가야 할 때만 채운다.
	TArray<FModifiedVoxelValue> ModifiedValues;
	bool bUseModifiedValuesForVolume = false;
};

// Voxel value/material 표현 편집만 담당한다. 원본 amount와 ownership은 Subsystem이 관리한다.
class DEEPRAIDERS_API FDRSnowSurfaceEditor
{
public:
	// Request에 TargetVoxelWorld가 없을 때 사용할 fallback World다.
	void SetWorld(UWorld* InWorld)
	{
		World = InWorld;
	}

	FDRSnowSurfaceEditResult AddSnowAtArea(const FDRSnowSurfaceAddRequest& Request);
	bool AddDirectionalSnowAtAreaAsync(
		const FDRSnowSurfaceAddRequest& Request,
		TFunction<void(FDRSnowSurfaceEditResult&&)> Completion);
	FDRSnowSurfaceEditResult RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request);
	// ownership을 우선하고, ownership이 없는 표면만 Volume의 우세 팀으로 다시 칠한다.
	bool RepaintSnowMaterialsAtArea(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowOwnershipStore& OwnershipStore,
		const FDRSnowVolumeStore& VolumeStore);

private:
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const;
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;

	UWorld* World = nullptr;
};
