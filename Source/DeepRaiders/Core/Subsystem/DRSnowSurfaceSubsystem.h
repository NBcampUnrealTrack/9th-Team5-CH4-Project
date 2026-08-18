#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRSnowSurfaceSubsystem.generated.h"

class AVoxelWorld;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowRemovedFromSurfaceDelegate,
	const FDRSnowSurfaceRemoveRequest&,
	float);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowAddedToSurfaceDelegate,
	const FDRSnowSurfaceAddRequest&,
	float);

UCLASS()
class DEEPRAIDERS_API UDRSnowSurfaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	float AddSnowAtArea(const FDRSnowSurfaceAddRequest& Request);

	float RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	// 현재 Voxel 표면을 다시 찾고, SnowVolume의 dominant team 기준으로 material index를 복원한다.
	// RemoveSnow로 원본 density를 줄인 뒤 호출해야 새로 드러난 표면 색이 맞는다.
	bool RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	FDRSnowAddedToSurfaceDelegate OnSnowAddedToSurface;
	FDRSnowRemovedFromSurfaceDelegate OnSnowRemovedFromSurface;

private:
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const;
	
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;
};
