#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "VoxelTools/Tools/VoxelToolBase.h"
#include "VoxelTools/VoxelSurfaceEdits.h"
#include "DRDirectionalSurfaceTool.generated.h"

class AVoxelWorld;

// SurfaceTool의 브러시 모양은 재사용하되, 최종 write는 add/remove 방향으로만 합성하는 Voxel Tool이다.
// 겹쳐 쏜 눈이 기존 팀 영역을 밀어 섞지 않도록 Snow 시스템에서 사용한다.
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

	// Processed surface 결과를 실제 voxel 값에 반영한다.
	// Add는 더 filled 되는 값만, Remove는 더 empty 되는 값만 적용한다.
	static float ApplySurfaceVolumeEdit(
		AVoxelWorld* VoxelWorld,
		const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
		float DistanceDivisor,
		bool bAdd,
		TArray<FModifiedVoxelValue>& ModifiedValues,
		FVoxelIntBox& EditedBounds,
		bool bUpdateRender = true);

	// 실제로 값이 변한 voxel 위치만 material paint 입력으로 변환한다.
	static FVoxelSurfaceEditsProcessedVoxels MakeModifiedValueVoxelGroup(
		const FVoxelIntBox& Bounds,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		bool bAdd);
};
