#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"

class AVoxelWorld;
class FDRSnowOwnershipStore;
class FDRSnowVolumeStore;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowRemovedFromSurfaceDelegate,
	const FDRSnowSurfaceRemoveRequest&,
	float);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowAddedToSurfaceDelegate,
	const FDRSnowSurfaceAddRequest&,
	float);

// Voxel value/material 표현 편집만 담당한다. 원본 amount와 ownership은 store가 관리한다.
class DEEPRAIDERS_API FDRSnowSurfaceEditor
{
public:
	void Configure(
		UWorld* InWorld,
		FDRSnowVolumeStore& InVolumeStore,
		FDRSnowOwnershipStore& InOwnershipStore)
	{
		World = InWorld;
		VolumeStore = &InVolumeStore;
		OwnershipStore = &InOwnershipStore;
	}

	float AddSnowAtArea(const FDRSnowSurfaceAddRequest& Request);
	float RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request);
	bool RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	FDRSnowAddedToSurfaceDelegate OnSnowAddedToSurface;
	FDRSnowRemovedFromSurfaceDelegate OnSnowRemovedFromSurface;

private:
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const;
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;

	UWorld* World = nullptr;
	FDRSnowVolumeStore* VolumeStore = nullptr;
	FDRSnowOwnershipStore* OwnershipStore = nullptr;
};
