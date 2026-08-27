#pragma once

#include "CoreMinimal.h"
#include "VoxelTools/VoxelSurfaceEdits.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "VoxelTools/Tools/VoxelToolBase.h"
#include "DRSnowAbsorbTool.generated.h"

class AVoxelWorld;

// 청소기 흡수 범위처럼 시작점은 좁고 끝점은 넓은 frustum 형태로 표면의 눈을 점진적으로 제거한다.
// 게임플레이에서는 RemoveSnowFromFrustum을 사용하고, Voxel Tool 목록에서는 형태/감쇠를 직접 확인할 수 있다.
UCLASS()
class DEEPRAIDERS_API UDRSnowAbsorbTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UDRSnowAbsorbTool();

	virtual void GetToolConfig(FVoxelToolBaseConfig& OutConfig) const override;
	virtual FVoxelIntBoxWithValidity DoEdit() override;

	// 흡수구 쪽의 반경 비율이다. 0이면 뾰족한 원뿔이 된다.
	UPROPERTY(Category = "Snow Absorb Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InnerRadiusRatio = 0.2f;

	// 가장 먼 끝에서의 제거 강도 비율이다. 작을수록 가까운 눈부터 먼저 빨려 들어간다.
	UPROPERTY(Category = "Snow Absorb Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FarStrengthRatio = 0.2f;

	UPROPERTY(Category = "Snow Absorb Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float Strength = 1.f;

	UPROPERTY(Category = "Snow Absorb Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float DistanceDivisor = 4.f;

	static float RemoveSnowFromFrustum(
		AVoxelWorld* VoxelWorld,
		const FVector& BrushOrigin,
		const FVector& TargetLocation,
		float OuterRadius,
		float InnerRadiusRatio,
		float FarStrengthRatio,
		float Strength,
		float DistanceDivisor,
		TArray<FModifiedVoxelValue>& OutModifiedValues,
		FVoxelIntBox& OutEditedBounds,
		bool bUpdateRender = true);

	// 런타임 흡수는 거대한 frustum 편집 대신 이 작은 Volume 샘플 하나만 사용한다.
	static float RemoveSnowAtSample(
		AVoxelWorld* VoxelWorld,
		const FVector& SampleLocation,
		float Radius,
		TArray<FModifiedVoxelValue>& OutModifiedValues,
		FVoxelIntBox& OutEditedBounds,
		bool bUpdateRender = true);

	static float RemoveSnowFromFrustumSlice(
		AVoxelWorld* VoxelWorld,
		const FVector& FrustumOrigin,
		const FVector& FrustumDirection,
		float FrustumRange,
		float OuterRadius,
		float SliceStart,
		float SliceLength,
		float Strength,
		float DistanceDivisor,
		TArray<FModifiedVoxelValue>& OutModifiedValues,
		FVoxelIntBox& OutEditedBounds,
		bool bUpdateRender = true);
};
