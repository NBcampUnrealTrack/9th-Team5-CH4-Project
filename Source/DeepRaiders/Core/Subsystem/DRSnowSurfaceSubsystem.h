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

	// 현재 Voxel 표면을 다시 찾고, DirectionalSurfaceTool ownership을 우선 사용해 material index를 복원한다.
	// ownership이 없는 위치만 SnowVolume dominant team을 fallback으로 사용한다.
	bool RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	FDRSnowAddedToSurfaceDelegate OnSnowAddedToSurface;
	FDRSnowRemovedFromSurfaceDelegate OnSnowRemovedFromSurface;

private:
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const;
	
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;
};
