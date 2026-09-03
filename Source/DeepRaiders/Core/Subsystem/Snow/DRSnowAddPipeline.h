#pragma once

#include "CoreMinimal.h"
#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

class AVoxelWorld;
class FDRSnowOwnershipStore;
class FDRSnowSurfaceEditor;
class FDRSnowVolumeStore;
class FDRSnowVoxelContainmentEvaluator;
class UWorld;

// 눈 추가의 Voxel 편집과 Ownership/Volume 반영 순서를 동기적으로 조율한다.
class DEEPRAIDERS_API FDRSnowAddPipeline
{
public:
	FDRSnowAddPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowOwnershipStore& InOwnershipStore,
		FDRSnowVolumeStore& InVolumeStore,
		FDRSnowVoxelContainmentEvaluator& InContainmentEvaluator);

	FDRSnowAddResult Execute(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request);
	FDRSnowAddResult Replay(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request,
		float AuthoritativeAmount);

private:
	FDRSnowAddResult ExecuteDirectionalAdd(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request,
		float AuthoritativeAmount = -1.f);
	void CommitAddedSurfaceEdit(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		float VolumeAmount = -1.f);
	void AddVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceAddRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxAddedAmount);

	FDRSnowSurfaceEditor& SurfaceEditor;
	FDRSnowOwnershipStore& OwnershipStore;
	FDRSnowVolumeStore& VolumeStore;
	FDRSnowVoxelContainmentEvaluator& ContainmentEvaluator;
};
