#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

#include "DRSnowSnapshotSerializer.generated.h"

class AVoxelWorld;
class FDRSnowVolumeStore;

struct FDRSnowJoinCheckpoint
{
	// OperationSequence 이후의 recent history를 재생하기 위한 checkpoint 기준점이다.
	int32 SnapshotId = 0;
	int32 OperationSequence = 0;
	FName VoxelWorldName = NAME_None;
	TArray<uint8> VoxelSaveData;
	TArray<uint8> SnowVolumeData;

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
	int64 TotalCompressedBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Join Snapshot")
	float TotalCompressedMB = 0.f;
};

class DEEPRAIDERS_API FDRSnowSnapshotSerializer
{
public:
	explicit FDRSnowSnapshotSerializer(FDRSnowVolumeStore& InVolumeStore)
		: VolumeStore(InVolumeStore)
	{
	}

	FDRJoinSnapshotSizeReport MeasureCompressedSnapshotSize(
		AVoxelWorld* TargetVoxelWorld = nullptr,
		bool bLogResult = true) const;

	// 서버의 현재 VoxelWorld 및 SnowVolume 상태를 하나의 checkpoint로 고정한다.
	bool CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld = nullptr);
	// 매 눈 작업마다 checkpoint 전체 데이터를 복사하지 않도록 Sequence만 조회한다.
	bool GetLatestCheckpointOperationSequence(int32& OutOperationSequence) const;
	bool GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint) const;
	bool GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint) const;
	void ResetCheckpoints();

	// 클라이언트가 받은 checkpoint를 적용한다. VoxelWorld 생성 전이면 false를 반환한다.
	bool ApplyCheckpoint(
		FName VoxelWorldName,
		const TArray<uint8>& VoxelSaveData,
		const TArray<uint8>& SnowVolumeData);

	// serializer는 UObject가 아니므로 VoxelWorld 탐색에 쓸 World context를 호출 전에 받는다.
	void SetWorld(UWorld* InWorld)
	{
		World = InWorld;
	}

private:
	static constexpr int32 SnowVolumeSnapshotVersion = 2;
	static float BytesToMB(int64 Bytes);
	static void SerializeSnowCell(
		FArchive& Archive,
		const FIntVector& LocalCell,
		const FDRSnowVolumeChunk& Chunk,
		int32 LocalIndex);
	static bool DeserializeSnowCell(
		FArchive& Archive,
		FIntVector& OutLocalCell,
		FDRSnowVolumeChunk& OutChunk);
	AVoxelWorld* ResolveVoxelWorld(AVoxelWorld* TargetVoxelWorld) const;
	FDRSnapshotVoxelSaveSizeReport MeasureVoxelSave(AVoxelWorld* TargetVoxelWorld) const;
	FDRSnapshotSnowVolumeSizeReport MeasureSnowVolume() const;
	// 크기 측정과 실제 checkpoint 저장이 동일한 sparse Volume 바이트 포맷을 사용하도록 한다.
	// OutSizeReport가 있으면 직렬화 중 집계 정보도 함께 채운다.
	void SerializeSnowVolumePayload(
		FArchive& Archive,
		FDRSnapshotSnowVolumeSizeReport* OutSizeReport) const;
	bool SerializeSnowVolume(TArray<uint8>& OutCompressedData) const;
	bool DeserializeSnowVolume(const TArray<uint8>& CompressedData);

	int32 NextSnapshotId = 1;
	int32 LatestCheckpointId = INDEX_NONE;
	TMap<int32, FDRSnowJoinCheckpoint> CheckpointsById;
	UWorld* World = nullptr;
	FDRSnowVolumeStore& VolumeStore;
};
