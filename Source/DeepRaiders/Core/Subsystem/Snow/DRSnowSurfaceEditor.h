#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRDirectionalSurfaceTool.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "VoxelTools/VoxelSurfaceTools.h"

class AVoxelWorld;

// Voxel 표현 편집의 실제 결과다. 점령 부피 갱신은 Pipeline이 이 결과를 사용해 처리한다.
struct FDRSnowSurfaceEditResult
{
	// 요청량이 아니라 Voxel 값이 실제로 변한 양이다.
	float AppliedAmount = 0.f;
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;
	FVoxelIntBox EditedBounds;
	// 실제 추가/제거 위치를 재질 채우기와 점령 부피 갱신에 사용한다.
	TArray<FModifiedVoxelValue> ModifiedValues;
	// DirectionalSurfaceTool처럼 Volume도 실제 변경 위치를 따라가야 하는 경로만 true다.
	bool bUseModifiedValuesForVolume = false;
	// Only the combined Directional path sets this; other tools still use FillAddedSnowMaterials.
	bool bAddedMaterialsFinalized = false;
	double FootprintMs = 0.0;
	// Legacy async material dispatch -> its game-thread callback (includes waiting).
	double LegacyMaterialMs = 0.0;
	TSharedPtr<FDRDirectionalSurfaceEditTimings, ESPMode::ThreadSafe> DirectionalTimings;
};

// Voxel value/material 표현 편집만 담당한다. 점령 부피는 Pipeline이 관리한다.
class DEEPRAIDERS_API FDRSnowSurfaceEditor
{
public:
	static bool IsCombinedDirectionalEditEnabled();
	static bool IsDirectionalPerfLoggingEnabled();
	// Request에 TargetVoxelWorld가 없을 때 사용할 fallback World다.
	void SetWorld(UWorld* InWorld);

	FDRSnowSurfaceEditResult AddSnowAtArea(const FDRSnowSurfaceAddRequest& Request);
	// 실제 새로 추가한 복셀 전체에 재질을 저장해 내부 색을 유지한다.
	void FillAddedSnowMaterials(const FDRSnowSurfaceEditResult& EditResult, int32 TeamId);
	bool AddDirectionalSnowAtAreaAsync(
		const FDRSnowSurfaceAddRequest& Request,
		TFunction<void(FDRSnowSurfaceEditResult&&)> Completion);

	// 눈총 frustum 전용 제거 경로다. 일반 아이템 제거에서는 사용하지 않는다.
	FDRSnowSurfaceEditResult RemoveSnowWithAbsorbTool(const FDRSnowSurfaceRemoveRequest& Request);
	FDRSnowSurfaceEditResult RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const;
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;

private:
	UWorld* World = nullptr;
};
