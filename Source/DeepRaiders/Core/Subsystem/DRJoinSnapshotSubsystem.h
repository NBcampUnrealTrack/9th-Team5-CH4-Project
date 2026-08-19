#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRJoinSnapshotSubsystem.generated.h"

class AVoxelWorld;

struct FDRSnowJoinCheckpoint
{
	int32 SnapshotId = 0;
	int32 OperationSequence = 0;
	FName VoxelWorldName = NAME_None;
	TArray<uint8> VoxelSaveData;
	TArray<uint8> SnowVolumeData;
	TArray<uint8> OwnershipData;

	bool IsValid() const
	{
		return SnapshotId > 0 && !VoxelSaveData.IsEmpty();
	}
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnapshotVoxelSaveSizeReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	FName VoxelWorldName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int64 CompressedSerializedBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float CompressedSerializedMB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int32 ObjectCount = 0;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnapshotSnowVolumeSizeReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int32 ChunkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int32 NonEmptyCellCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int64 SparseSerializedBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int64 CompressedSparseBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float SparseSerializedMB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float CompressedSparseMB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float CompressionRatio = 0.f;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRJoinSnapshotSizeReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	FDRSnapshotVoxelSaveSizeReport VoxelSave;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	FDRSnapshotSnowVolumeSizeReport SnowVolume;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int64 OwnershipCompressedBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float OwnershipCompressedMB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	int64 TotalCompressedBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float TotalCompressedMB = 0.f;
};

UCLASS()
class DEEPRAIDERS_API UDRJoinSnapshotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintCallable, Category = "Join Snapshot")
	FDRJoinSnapshotSizeReport MeasureCompressedSnapshotSize(
		AVoxelWorld* TargetVoxelWorld = nullptr,
		bool bLogResult = true) const;

	// 서버의 현재 VoxelWorld 및 SnowVolume 상태를 하나의 checkpoint로 고정한다.
	bool CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld = nullptr);
	bool GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint) const;
	bool GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint) const;

	// 클라이언트가 받은 checkpoint를 적용한다. VoxelWorld 생성 전이면 false를 반환한다.
	bool ApplyCheckpoint(
		FName VoxelWorldName,
		const TArray<uint8>& VoxelSaveData,
		const TArray<uint8>& SnowVolumeData,
		const TArray<uint8>& OwnershipData);

private:
	AVoxelWorld* ResolveVoxelWorld(AVoxelWorld* TargetVoxelWorld) const;
	FDRSnapshotVoxelSaveSizeReport MeasureVoxelSave(AVoxelWorld* TargetVoxelWorld) const;
	FDRSnapshotSnowVolumeSizeReport MeasureSnowVolume() const;
	bool SerializeSnowVolume(TArray<uint8>& OutCompressedData) const;
	bool DeserializeSnowVolume(const TArray<uint8>& CompressedData);
	bool SerializeOwnership(
		AVoxelWorld* VoxelWorld,
		TArray<uint8>& OutCompressedData) const;
	bool DeserializeOwnership(
		AVoxelWorld* VoxelWorld,
		const TArray<uint8>& CompressedData);

	int32 NextSnapshotId = 1;
	FDRSnowJoinCheckpoint LatestCheckpoint;
	TMap<int32, FDRSnowJoinCheckpoint> CheckpointsById;
};
