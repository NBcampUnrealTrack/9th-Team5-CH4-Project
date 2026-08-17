#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelTools/VoxelSurfaceEdits.h"
#include "DRVoxelTeamColorLibrary.generated.h"

class AVoxelWorld;

UCLASS()
class DEEPRAIDERS_API UDRVoxelTeamColorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 임의 위치의 현재 표면을 찾아 팀 material index로 칠하는 테스트/도구용 함수다.
	UFUNCTION(BlueprintCallable, Category = "Voxel|Team Color")
	static bool PaintTeamSurfaceAtArea(
		AVoxelWorld* VoxelWorld,
		FVector WorldLocation,
		float Radius,
		int32 TeamId);

	// 이미 계산된 surface edit voxels를 재사용해 팀 material index만 칠한다.
	// SnowSurfaceSubsystem처럼 값 편집과 재질 편집을 같은 bounds에서 처리할 때 사용한다.
	static bool PaintProcessedTeamSurface(
		AVoxelWorld* VoxelWorld,
		const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
		int32 TeamId,
		bool bUpdateRender = true);
};
