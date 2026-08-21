#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRVoxelTerrainQueryLibrary.generated.h"

class AVoxelWorld;

UENUM(BlueprintType)
enum class EDRVoxelDepositRequestPhase : uint8
{
	BuildCandidates,
	ApplyVoxels,
	Finished
};

USTRUCT(BlueprintType)
struct FDRVoxelCompressedValueDelta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 LocalIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 QuantizedValue = 0;
};

USTRUCT(BlueprintType)
struct FDRVoxelDepositDeltaRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMax = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	uint8 MaterialIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TArray<FDRVoxelCompressedValueDelta> Deltas;
};

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
	TArray<FIntVector> PendingVoxels;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextVoxelIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntPoint ScanCursor = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 VoxelSampleStep = 1;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	float DepositAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bOnlyTopSurface = true;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bUseJitteredSamples = true;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	float JitterRatio = 0.4f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 DepositPatchRadius = 1;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	float MinSurfaceDepositChance = 0.15f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	float MaxSurfaceDepositChance = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	float LowerSurfaceSelectionBias = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	EDRVoxelDepositRequestPhase Phase = EDRVoxelDepositRequestPhase::BuildCandidates;

	FRandomStream RandomStream;
	TSet<FIntVector> PendingVoxelPositions;
	TSet<FIntVector> WrittenVoxelPositions;
};

UCLASS()
class DEEPRAIDERS_API UDRVoxelTerrainQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Voxel Terrain|Query")
	static bool GetMaterialCountsInBox(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		const TArray<uint8>& TargetMaterialIndices,
		TMap<uint8, int32>& OutMaterialCounts,
		int32& OutTotalCount);

	UFUNCTION(BlueprintPure, Category="Voxel Terrain|Query")
	static bool IsVoxelUpdateInBox(
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FVector& Location,
		float Radius);

	static bool MakeDepositInBoxRequest(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		float DepositAmount,
		uint8 DepositMaterialIndex,
		int32 RandomSeed,
		bool bOnlyTopSurface,
		bool bUseJitteredSamples,
		float JitterRatio,
		int32 DepositPatchRadius,
		float MinSurfaceDepositChance,
		float MaxSurfaceDepositChance,
		float LowerSurfaceSelectionBias,
		FDRVoxelDepositInBoxRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool MakeDepositInBoxRequest(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		float DepositAmount,
		uint8 DepositMaterialIndex,
		int32 DepositPatchRadius,
		float MinSurfaceDepositChance,
		float MaxSurfaceDepositChance,
		float LowerSurfaceSelectionBias,
		int32 RandomSeed,
		FDRVoxelDepositInBoxRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool ProcessDepositInBoxRequestsTick(
		UPARAM(ref) TArray<FDRVoxelDepositInBoxRequest>& Requests,
		int32 MaxScanColumnsToProcess,
		int32 MaxVoxelsToProcess,
		int32& OutModifiedVoxelCount,
		int32& OutScannedColumnCount,
		FDRVoxelDepositDeltaRecord& OutDeltaRecord,
		int32& OutRemainingRequestCount);

	static bool ProcessDepositInBoxRequestsTick(
		TArray<FDRVoxelDepositInBoxRequest>& Requests,
		int32 MaxVoxelsToProcess,
		int32& OutModifiedVoxelCount,
		FDRVoxelDepositDeltaRecord& OutDeltaRecord,
		int32& OutRemainingRequestCount);

	static bool ProcessDepositInBoxRequestsTick(
		TArray<FDRVoxelDepositInBoxRequest>& Requests,
		int32 MaxVoxelsToProcess,
		int32& OutModifiedVoxelCount,
		int32& OutRemainingRequestCount);

	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool ApplyDepositDeltaRecord(
		AVoxelWorld* VoxelWorld,
		const FDRVoxelDepositDeltaRecord& DeltaRecord,
		int32& OutAppliedVoxelCount);
};
