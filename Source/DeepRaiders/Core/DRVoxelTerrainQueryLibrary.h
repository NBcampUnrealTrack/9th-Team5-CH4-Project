// DRVoxelTerrainQueryLibrary.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRVoxelTerrainQueryLibrary.generated.h"

class AVoxelWorld;

USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMax = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TArray<FIntPoint> PendingColumns;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextColumnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 VoxelSampleStep = 1;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 SmoothRadius = 1;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 MaxHeightStep = 1;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	float DepositAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bIsValid = false;

	TSet<FIntVector> WrittenVoxelPositions;
};

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

	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool MakeDepositInBoxRequest(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		float DepositAmount,
		uint8 DepositMaterialIndex,
		int32 SmoothRadius,
		int32 MaxHeightStep,
		int32 RandomSeed,
		FDRVoxelDepositInBoxRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool ProcessDepositInBoxRequestsTick(
		UPARAM(ref) TArray<FDRVoxelDepositInBoxRequest>& Requests,
		int32 MaxColumnsToProcess,
		int32& OutModifiedVoxelCount,
		int32& OutRemainingRequestCount);
};