#pragma once

#include "CoreMinimal.h"
#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

class AVoxelWorld;
class FDRSnowOwnershipStore;
class FDRSnowSurfaceEditor;
class FDRSnowVolumeStore;
class UWorld;

enum class EDRSnowRemovalPath : uint8
{
	Standard,
	Absorb
};

struct FDRSnowRemovalExecutionResult
{
	FDRSnowRemoveResult RemoveResult;
	FDRSnowMaterialPatch MaterialPatch;
};

struct FDRSnowRemovalReplayResult
{
	bool bApplied = false;
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;
};

// 눈 제거의 전체 도메인 순서를 소유한다.
// Surface 편집 → 서버 원본 갱신 → 재질 해석/적용 순서가 이 클래스 밖으로 흩어지지 않는다.
class DEEPRAIDERS_API FDRSnowRemovalPipeline
{
public:
	FDRSnowRemovalPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowOwnershipStore& InOwnershipStore,
		FDRSnowVolumeStore& InVolumeStore);

	FDRSnowRemovalExecutionResult Execute(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath,
		bool bBuildMaterialPatch);

	FDRSnowRemovalReplayResult Replay(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		float AuthoritativeAmount,
		EDRSnowRemovalPath RemovalPath);

private:
	FDRSnowSurfaceEditResult RemoveSurface(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;
	bool ResolveMaterials(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		EDRSnowRemovalPath RemovalPath,
		FDRSnowResolvedMaterialEdit& OutResolvedEdit) const;
	void ApplyRemovedSurfaceEdit(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		float VolumeAmount);
	void RemoveVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceRemoveRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxRemovedAmount);

	FDRSnowSurfaceEditor& SurfaceEditor;
	FDRSnowOwnershipStore& OwnershipStore;
	FDRSnowVolumeStore& VolumeStore;
};
