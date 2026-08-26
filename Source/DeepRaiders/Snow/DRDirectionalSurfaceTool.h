#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "VoxelTools/Tools/VoxelToolBase.h"
#include "VoxelTools/VoxelSurfaceEdits.h"
#include "DRDirectionalSurfaceTool.generated.h"

class AVoxelWorld;

using FDRDirectionalSurfaceEditComplete =
	TFunction<void(TArray<FModifiedVoxelValue>&&, FVoxelIntBox)>;

// SurfaceTool의 브러시 모양은 재사용하되, 밀어낼 surface shell을 실제 이동시키지 않고
// 이번 이동이 지나갈 swept volume만 stamp처럼 add/remove 합성하는 Voxel Tool이다.
UCLASS()
class DEEPRAIDERS_API UDRDirectionalSurfaceTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UDRDirectionalSurfaceTool();

	//~ Begin UVoxelToolBase Interface
	virtual void GetToolConfig(FVoxelToolBaseConfig& OutConfig) const override;
	virtual FVoxelIntBoxWithValidity DoEdit() override;
	//~ End UVoxelToolBase Interface

public:
	// true면 눈/땅을 채우고, false면 비운다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite)
	bool bAdd = true;

	// SurfaceTool의 push/pull 세기와 같은 의미다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float Strength = 1.f;

	// 브러시 가장자리 감쇠 비율이다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0", UIMax = "1"))
	float Falloff = 0.35f;

	// SurfaceTool target 값을 voxel density 값으로 줄이는 비율이다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float DistanceDivisor = 4.f;

public:
	// SurfaceTool과 같은 표면 후보, falloff, strength 결과를 만든다.
	// 이 단계에서는 아직 voxel 값을 수정하지 않는다.
	static FVoxelSurfaceEditsProcessedVoxels FindSurfaceFootprint(
		AVoxelWorld* VoxelWorld,
		const FVector& WorldLocation,
		float Radius,
		float Falloff,
		float Strength,
		bool bAdd);

	// Voxel 표면이 아닌 StaticMesh 등의 Hit 표면을 plane distance field처럼 취급해 footprint를 만든다.
	static FVoxelSurfaceEditsProcessedVoxels MakeVirtualSurfaceFootprint(
		AVoxelWorld* VoxelWorld,
		const FVector& WorldLocation,
		const FVector& SurfaceNormal,
		float Radius,
		float Falloff,
		float Strength,
		bool bAdd);

	// Processed surface 결과에서 0면을 가로지르는 swept volume만 실제 voxel 값에 반영한다.
	// Add는 비어 있던 stamp 부피를 채우고, Remove는 해당 stamp 부피를 비운다.
	static float ApplySurfaceVolumeEdit(
		AVoxelWorld* VoxelWorld,
		const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
		float DistanceDivisor,
		bool bAdd,
		TArray<FModifiedVoxelValue>& ModifiedValues,
		FVoxelIntBox& EditedBounds,
		bool bUpdateRender = true);

	// density 쓰기를 VoxelWorld 작업 풀에서 실행하고 게임 스레드에서 완료 콜백을 호출한다.
	static bool ApplySurfaceVolumeEditAsync(
		AVoxelWorld* VoxelWorld,
		const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
		float DistanceDivisor,
		bool bAdd,
		FDRDirectionalSurfaceEditComplete Completion);

	// 실제로 값이 변한 voxel 위치만 material paint 입력으로 변환한다.
	static FVoxelSurfaceEditsProcessedVoxels MakeModifiedValueVoxelGroup(
		const FVoxelIntBox& Bounds,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		bool bAdd);

private:
	static float GetModifiedValueAmount(const TArray<FModifiedVoxelValue>& ModifiedValues);
	static float GetSurfaceToolTargetValue(const FVoxelSurfaceEditsVoxel& SurfaceVoxel, float DistanceDivisor);
	static bool IsInsideSweptSurfaceVolume(const FVoxelSurfaceEditsVoxel& SurfaceVoxel, bool bAdd);
};
