#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "VoxelTools/VoxelSurfaceTools.h"

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
	// 서버 Ownership 원본이 실제로 추가/제거된 위치를 기록할 수 있도록 채운다.
	TArray<FModifiedVoxelValue> ModifiedValues;
	// DirectionalSurfaceTool처럼 Volume도 실제 변경 위치를 따라가야 하는 경로만 true다.
	bool bUseModifiedValuesForVolume = false;
};

// 서버가 Ownership/Volume으로 결정한 최종 MaterialIndex와 실제 paint 입력을 보관한다.
// 네트워크 패치는 paint 이후 실제로 바뀐 voxel만 사용해 별도로 만든다.
struct FDRSnowResolvedMaterialGroup
{
	uint8 MaterialIndex = 0;
	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
};

struct FDRSnowResolvedMaterialEdit
{
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;
	TArray<FDRSnowResolvedMaterialGroup> Groups;

	bool IsEmpty() const { return Groups.IsEmpty(); }
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
	// 눈총 frustum 전용 제거 경로다. 일반 아이템 제거에서는 사용하지 않는다.
	FDRSnowSurfaceEditResult RemoveSnowWithAbsorbTool(const FDRSnowSurfaceRemoveRequest& Request);
	FDRSnowSurfaceEditResult RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request);
	// 서버 원본 Ownership/Volume을 읽어 최종 MaterialIndex 그룹을 만든다.
	bool ResolveSnowMaterialsAtArea(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		const FDRSnowOwnershipStore& OwnershipStore,
		const FDRSnowVolumeStore& VolumeStore,
		FDRSnowResolvedMaterialEdit& OutResolvedEdit);
	bool ResolveSnowMaterialsAtModifiedVoxels(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		const FDRSnowOwnershipStore& OwnershipStore,
		const FDRSnowVolumeStore& VolumeStore,
		FDRSnowResolvedMaterialEdit& OutResolvedEdit);
	// Resolve 결과를 서버 VoxelWorld에 적용한다. OutMaterialPatch에는 실제 변경분만 기록한다.
	bool ApplyResolvedSnowMaterials(
		const FDRSnowResolvedMaterialEdit& ResolvedEdit,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);
	// 이후 클라이언트 전환에서 사용할 MaterialIndex Patch 적용 경계다.
	bool ApplySnowMaterialPatch(AVoxelWorld* VoxelWorld, const FDRSnowMaterialPatch& MaterialPatch);
	// 정확한 patch 좌표를 작업 스레드에서 순차 적용하고, 완료 시 편집된 청크 Bounds를 반환한다.
	bool ApplySnowMaterialPatchAsync(
		AVoxelWorld* VoxelWorld,
		FDRSnowMaterialPatch MaterialPatch,
		TFunction<void(bool, TArray<FVoxelIntBox>&&)> Completion);
	// ownership을 우선하고, ownership이 없는 표면만 Volume의 우세 팀으로 다시 칠한다.
	bool RepaintSnowMaterialsAtArea(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		const FDRSnowOwnershipStore& OwnershipStore,
		const FDRSnowVolumeStore& VolumeStore);
	// 흡수처럼 연속 변경되는 경우, 실제로 비워진 Voxel만 즉시 재질 갱신한다.
	bool RepaintSnowMaterialsAtModifiedVoxels(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		const FDRSnowOwnershipStore& OwnershipStore,
		const FDRSnowVolumeStore& VolumeStore);

private:
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const;
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;

	UWorld* World = nullptr;
};
