// DRVoxelTerrainQueryLibrary.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRVoxelTerrainQueryLibrary.generated.h"

class AVoxelWorld;

UCLASS()
class DEEPRAIDERS_API UDRVoxelTerrainQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Voxel Terrain|Query")
	static bool GetMaterialCountsInBox(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		const TArray<uint8>& TargetMaterialIndices,
		TMap<uint8, int32>& OutMaterialCounts,
		int32& OutTotalCount);

	UFUNCTION(BlueprintPure, Category = "Voxel Terrain|Query")
	static bool IsVoxelUpdateInBox(
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FVector& Location,
		float Radius);

	UFUNCTION(BlueprintCallable, Category = "Voxel Terrain|Snow")
	static bool AddSnowInBox(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		float SnowAmount,
		uint8 SnowMaterialIndex,
		int32& OutModifiedVoxelCount);
};