#include "DRSnowSnapshotSerializer.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "EngineUtils.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelUtilities/VoxelSerializationUtilities.h"
#include "VoxelWorld.h"

float FDRSnowSnapshotSerializer::BytesToMB(const int64 Bytes)
{
	return static_cast<float>(static_cast<double>(Bytes) / static_cast<double>(1 << 20));
}

void FDRSnowSnapshotSerializer::SerializeSnowCell(
	FArchive& Archive,
	const FIntVector& LocalCell,
	const FDRSnowVolumeChunk& Chunk,
	const int32 LocalIndex)
{
	// Chunk palette를 먼저 저장한 뒤, 그 순서대로 cell의 team amount를 기록한다.
	FIntVector MutableLocalCell = LocalCell;
	float NeutralAmount = Chunk.Cells[LocalIndex].NeutralAmount;
	Archive << MutableLocalCell;
	Archive << NeutralAmount;
	for (int32 TeamSlot = 0; TeamSlot < Chunk.TeamIds.Num(); ++TeamSlot)
	{
		float TeamAmount = Chunk.GetTeamAmount(LocalIndex, TeamSlot);
		Archive << TeamAmount;
	}
}

bool FDRSnowSnapshotSerializer::DeserializeSnowCell(
	FArchive& Archive,
	FIntVector& OutLocalCell,
	FDRSnowVolumeChunk& OutChunk)
{
	// 읽는 쪽도 Chunk.TeamIds 순서를 그대로 사용해야 team amount가 올바른 팀에 복원된다.
	Archive << OutLocalCell;
	int32 LocalIndex = INDEX_NONE;
	if (!OutChunk.GetLocalIndex(OutLocalCell, LocalIndex))
	{
		return false;
	}

	Archive << OutChunk.Cells[LocalIndex].NeutralAmount;
	for (int32 TeamSlot = 0; TeamSlot < OutChunk.TeamIds.Num(); ++TeamSlot)
	{
		float TeamAmount = 0.f;
		Archive << TeamAmount;
		OutChunk.SetTeamAmount(LocalIndex, TeamSlot, TeamAmount);
	}

	return !Archive.IsError();
}

FDRJoinSnapshotSizeReport FDRSnowSnapshotSerializer::MeasureCompressedSnapshotSize(
	AVoxelWorld* TargetVoxelWorld,
	bool bLogResult) const
{
	FDRJoinSnapshotSizeReport Report;

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(TargetVoxelWorld);
	Report.VoxelSave = MeasureVoxelSave(VoxelWorld);
	Report.SnowVolume = MeasureSnowVolume();
	TArray<uint8> OwnershipData;
	if (SerializeOwnership(VoxelWorld, OwnershipData))
	{
		Report.OwnershipCompressedBytes = OwnershipData.Num();
		Report.OwnershipCompressedMB = BytesToMB(Report.OwnershipCompressedBytes);
	}
	Report.TotalCompressedBytes =
		Report.VoxelSave.CompressedSerializedBytes +
		Report.SnowVolume.CompressedSparseBytes +
		Report.OwnershipCompressedBytes;
	Report.TotalCompressedMB = BytesToMB(Report.TotalCompressedBytes);
	Report.bSuccess = Report.VoxelSave.bSuccess || Report.SnowVolume.bSuccess;

	if (bLogResult)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[JoinSnapshot][Measure] Success=%d TotalCompressed=%lld bytes (%.3f MB)"),
			Report.bSuccess,
			Report.TotalCompressedBytes,
			Report.TotalCompressedMB);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[JoinSnapshot][Voxel] Success=%d World=%s CompressedSerialized=%lld bytes (%.3f MB) Objects=%d"),
			Report.VoxelSave.bSuccess,
			*Report.VoxelSave.VoxelWorldName.ToString(),
			Report.VoxelSave.CompressedSerializedBytes,
			Report.VoxelSave.CompressedSerializedMB,
			Report.VoxelSave.ObjectCount);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[JoinSnapshot][SnowVolume] Success=%d Chunks=%d NonEmptyCells=%d Sparse=%lld bytes (%.3f MB) Compressed=%lld bytes (%.3f MB) Ratio=%.2f%%"),
			Report.SnowVolume.bSuccess,
			Report.SnowVolume.ChunkCount,
			Report.SnowVolume.NonEmptyCellCount,
			Report.SnowVolume.SparseSerializedBytes,
			Report.SnowVolume.SparseSerializedMB,
			Report.SnowVolume.CompressedSparseBytes,
			Report.SnowVolume.CompressedSparseMB,
			Report.SnowVolume.CompressionRatio * 100.f);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[JoinSnapshot][Ownership] Compressed=%lld bytes (%.3f MB)"),
			Report.OwnershipCompressedBytes,
			Report.OwnershipCompressedMB);
	}

	return Report;
}

bool FDRSnowSnapshotSerializer::CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld)
{
	UWorld* LocalWorld = World;
	if (!IsValid(LocalWorld) || LocalWorld->GetNetMode() == NM_Client)
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(TargetVoxelWorld);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FVoxelCompressedWorldSave CompressedSave;
	UVoxelDataTools::GetCompressedSave(VoxelWorld, CompressedSave);

	FBufferArchive VoxelArchive;
	CompressedSave.Serialize(VoxelArchive);
	if (VoxelArchive.Num() <= 0)
	{
		return false;
	}

	TArray<uint8> SnowVolumeData;
	if (!SerializeSnowVolume(SnowVolumeData))
	{
		return false;
	}
	TArray<uint8> OwnershipData;
	if (!SerializeOwnership(VoxelWorld, OwnershipData))
	{
		return false;
	}

	LatestCheckpoint.SnapshotId = NextSnapshotId++;
	LatestCheckpoint.OperationSequence = OperationSequence;
	LatestCheckpoint.VoxelWorldName = VoxelWorld->GetFName();
	LatestCheckpoint.VoxelSaveData = MoveTemp(VoxelArchive);
	LatestCheckpoint.SnowVolumeData = MoveTemp(SnowVolumeData);
	LatestCheckpoint.OwnershipData = MoveTemp(OwnershipData);
	CheckpointsById.Add(LatestCheckpoint.SnapshotId, LatestCheckpoint);
	while (CheckpointsById.Num() > 4)
	{
		int32 OldestSnapshotId = MAX_int32;
		for (const TPair<int32, FDRSnowJoinCheckpoint>& Pair : CheckpointsById)
		{
			OldestSnapshotId = FMath::Min(OldestSnapshotId, Pair.Key);
		}
		CheckpointsById.Remove(OldestSnapshotId);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[JoinSnapshot] Created Id=%d Sequence=%d Voxel=%d bytes SnowVolume=%d bytes Ownership=%d bytes"),
		LatestCheckpoint.SnapshotId,
		LatestCheckpoint.OperationSequence,
		LatestCheckpoint.VoxelSaveData.Num(),
		LatestCheckpoint.SnowVolumeData.Num(),
		LatestCheckpoint.OwnershipData.Num());
	return true;
}

bool FDRSnowSnapshotSerializer::GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint) const
{
	if (!LatestCheckpoint.IsValid())
	{
		return false;
	}

	OutCheckpoint = LatestCheckpoint;
	return true;
}

bool FDRSnowSnapshotSerializer::GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint) const
{
	const FDRSnowJoinCheckpoint* Checkpoint = CheckpointsById.Find(SnapshotId);
	if (!Checkpoint || !Checkpoint->IsValid())
	{
		return false;
	}

	OutCheckpoint = *Checkpoint;
	return true;
}

void FDRSnowSnapshotSerializer::ResetCheckpoints()
{
	LatestCheckpoint = {};
	CheckpointsById.Reset();
	NextSnapshotId = 1;
}

bool FDRSnowSnapshotSerializer::ApplyCheckpoint(
	FName VoxelWorldName,
	const TArray<uint8>& VoxelSaveData,
	const TArray<uint8>& SnowVolumeData,
	const TArray<uint8>& OwnershipData)
{
	AVoxelWorld* VoxelWorld = nullptr;
	if (VoxelWorldName.IsNone())
	{
		VoxelWorld = ResolveVoxelWorld(nullptr);
	}
	if (!VoxelWorldName.IsNone())
	{
		UWorld* LocalWorld = World;
		if (IsValid(LocalWorld))
		{
			for (TActorIterator<AVoxelWorld> It(LocalWorld); It; ++It)
			{
				if (IsValid(*It) && (*It)->GetFName() == VoxelWorldName)
				{
					VoxelWorld = *It;
					break;
				}
			}
		}
	}

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || VoxelSaveData.IsEmpty())
	{
		return false;
	}

	// Voxel 표현을 먼저 복원한 뒤, 그 표현의 팀 재질 판단에 쓰는 Store를 같은 checkpoint로 맞춘다.
	FMemoryReader VoxelReader(VoxelSaveData);
	FVoxelCompressedWorldSave CompressedSave;
	CompressedSave.Serialize(VoxelReader);
	if (VoxelReader.IsError() || !UVoxelDataTools::LoadFromCompressedSave(VoxelWorld, CompressedSave))
	{
		return false;
	}

	return DeserializeSnowVolume(SnowVolumeData) && DeserializeOwnership(VoxelWorld, OwnershipData);
}

AVoxelWorld* FDRSnowSnapshotSerializer::ResolveVoxelWorld(AVoxelWorld* TargetVoxelWorld) const
{
	if (IsValid(TargetVoxelWorld))
	{
		return TargetVoxelWorld;
	}

	UWorld* LocalWorld = World;
	if (!IsValid(LocalWorld))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(LocalWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}

FDRSnapshotVoxelSaveSizeReport FDRSnowSnapshotSerializer::MeasureVoxelSave(AVoxelWorld* TargetVoxelWorld) const
{
	FDRSnapshotVoxelSaveSizeReport Report;
	if (!IsValid(TargetVoxelWorld) || !TargetVoxelWorld->IsCreated())
	{
		return Report;
	}

	FVoxelCompressedWorldSave CompressedSave;
	UVoxelDataTools::GetCompressedSave(TargetVoxelWorld, CompressedSave);

	FBufferArchive Archive;
	CompressedSave.Serialize(Archive);

	Report.bSuccess = true;
	Report.VoxelWorldName = TargetVoxelWorld->GetFName();
	Report.CompressedSerializedBytes = Archive.Num();
	Report.CompressedSerializedMB = BytesToMB(Report.CompressedSerializedBytes);
	Report.ObjectCount = CompressedSave.Objects.Num();
	return Report;
}

FDRSnapshotSnowVolumeSizeReport FDRSnowSnapshotSerializer::MeasureSnowVolume() const
{
	FDRSnapshotSnowVolumeSizeReport Report;

	FBufferArchive SparseArchive;
	SerializeSnowVolumePayload(SparseArchive, &Report);

	TArray<uint8> CompressedData;
	FVoxelSerializationUtilities::CompressData(SparseArchive.GetData(), SparseArchive.Num(), CompressedData);

	Report.bSuccess = true;
	Report.SparseSerializedBytes = SparseArchive.Num();
	Report.CompressedSparseBytes = CompressedData.Num();
	Report.SparseSerializedMB = BytesToMB(Report.SparseSerializedBytes);
	Report.CompressedSparseMB = BytesToMB(Report.CompressedSparseBytes);
	Report.CompressionRatio =
		Report.SparseSerializedBytes > 0
			? static_cast<float>(
				static_cast<double>(Report.CompressedSparseBytes) /
				static_cast<double>(Report.SparseSerializedBytes))
			: 0.f;

	return Report;
}

void FDRSnowSnapshotSerializer::SerializeSnowVolumePayload(
	FArchive& Archive,
	FDRSnapshotSnowVolumeSizeReport* OutSizeReport) const
{
	FDRSnowVolumeSnapshot VolumeSnapshot;
	VolumeStore.CopySnapshotData(VolumeSnapshot);

	int32 Version = SnowVolumeSnapshotVersion;
	float CellSize = VolumeSnapshot.CellSize;
	int32 ChunkSize = VolumeSnapshot.ChunkSize;
	int32 ChunkCount = VolumeSnapshot.Chunks.Num();
	// v2부터 청크별 TeamIds palette를 포함한다. 이전 v1 Snapshot은 호환하지 않는다.
	Archive << Version;
	Archive << CellSize;
	Archive << ChunkSize;
	Archive << ChunkCount;

	if (OutSizeReport)
	{
		OutSizeReport->ChunkCount = ChunkCount;
	}

	for (const TPair<FIntVector, FDRSnowVolumeChunk>& Pair : VolumeSnapshot.Chunks)
	{
		const FDRSnowVolumeChunk& Chunk = Pair.Value;
		int32 NonEmptyCellCount = 0;
		for (int32 LocalIndex = 0; LocalIndex < Chunk.Cells.Num(); ++LocalIndex)
		{
			NonEmptyCellCount += Chunk.GetCellTotalAmount(LocalIndex) > 0.f ? 1 : 0;
		}

		FIntVector Origin = Chunk.Origin;
		TArray<int32> TeamIds = Chunk.TeamIds;
		Archive << Origin;
		Archive << TeamIds;
		Archive << NonEmptyCellCount;

		for (int32 LocalIndex = 0; LocalIndex < Chunk.Cells.Num(); ++LocalIndex)
		{
			if (Chunk.GetCellTotalAmount(LocalIndex) <= 0.f)
			{
				continue;
			}

			SerializeSnowCell(Archive, FIntVector(
				LocalIndex % Chunk.Size,
				(LocalIndex / Chunk.Size) % Chunk.Size,
				LocalIndex / (Chunk.Size * Chunk.Size)), Chunk, LocalIndex);

			if (OutSizeReport)
			{
				++OutSizeReport->NonEmptyCellCount;
			}
		}
	}
}

bool FDRSnowSnapshotSerializer::SerializeSnowVolume(TArray<uint8>& OutCompressedData) const
{
	OutCompressedData.Reset();
	FBufferArchive Archive;
	SerializeSnowVolumePayload(Archive, nullptr);

	FVoxelSerializationUtilities::CompressData(Archive.GetData(), Archive.Num(), OutCompressedData);
	return !OutCompressedData.IsEmpty();
}

bool FDRSnowSnapshotSerializer::DeserializeSnowVolume(const TArray<uint8>& CompressedData)
{
	if (CompressedData.IsEmpty())
	{
		return false;
	}

	TArray64<uint8> UncompressedData;
	if (!FVoxelSerializationUtilities::DecompressData(CompressedData, UncompressedData))
	{
		return false;
	}

	if (UncompressedData.Num() > MAX_int32)
	{
		return false;
	}

	TArray<uint8> ReaderData;
	ReaderData.Append(UncompressedData.GetData(), static_cast<int32>(UncompressedData.Num()));
	FMemoryReader Reader(ReaderData);
	int32 Version = 0;
	float CellSize = 0.f;
	int32 ChunkSize = 0;
	int32 ChunkCount = 0;
	Reader << Version;
	Reader << CellSize;
	Reader << ChunkSize;
	Reader << ChunkCount;
	if (Reader.IsError() || Version != SnowVolumeSnapshotVersion || CellSize <= 0.f || ChunkSize <= 0 || ChunkCount < 0)
	{
		return false;
	}

	TMap<FIntVector, FDRSnowVolumeChunk> RestoredChunks;
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		FIntVector Origin;
		TArray<int32> TeamIds;
		int32 NonEmptyCellCount = 0;
		Reader << Origin;
		Reader << TeamIds;
		Reader << NonEmptyCellCount;
		if (Reader.IsError() || TeamIds.Contains(INDEX_NONE) || NonEmptyCellCount < 0 || NonEmptyCellCount > ChunkSize * ChunkSize * ChunkSize)
		{
			return false;
		}

		FDRSnowVolumeChunk Chunk;
		Chunk.Initialize(Origin, ChunkSize, CellSize);
		Chunk.TeamIds = MoveTemp(TeamIds);
		Chunk.TeamAmounts.SetNumZeroed(Chunk.TeamIds.Num() * Chunk.Cells.Num());
		for (int32 CellIndex = 0; CellIndex < NonEmptyCellCount; ++CellIndex)
		{
			FIntVector LocalCell;
			if (!DeserializeSnowCell(Reader, LocalCell, Chunk))
			{
				return false;
			}
		}

		RestoredChunks.Add(Origin, MoveTemp(Chunk));
	}

	FDRSnowVolumeSnapshot VolumeSnapshot;
	VolumeSnapshot.CellSize = CellSize;
	VolumeSnapshot.ChunkSize = ChunkSize;
	VolumeSnapshot.Chunks = MoveTemp(RestoredChunks);
	VolumeStore.ReplaceSnapshotData(MoveTemp(VolumeSnapshot));
	return true;
}

bool FDRSnowSnapshotSerializer::SerializeOwnership(
	AVoxelWorld* VoxelWorld,
	TArray<uint8>& OutCompressedData) const
{
	OutCompressedData.Reset();
	if (!IsValid(VoxelWorld))
	{
		return false;
	}

	TMap<FIntVector, int32> TeamByVoxel;
	OwnershipStore.CopySnapshotData(VoxelWorld, TeamByVoxel);
	FBufferArchive Archive;
	int32 Version = OwnershipSnapshotVersion;
	int32 Count = TeamByVoxel.Num();
	Archive << Version;
	Archive << Count;
	for (const TPair<FIntVector, int32>& Pair : TeamByVoxel)
	{
		FIntVector VoxelPosition = Pair.Key;
		int32 TeamId = Pair.Value;
		Archive << VoxelPosition;
		Archive << TeamId;
	}

	FVoxelSerializationUtilities::CompressData(Archive.GetData(), Archive.Num(), OutCompressedData);
	return !OutCompressedData.IsEmpty();
}

bool FDRSnowSnapshotSerializer::DeserializeOwnership(
	AVoxelWorld* VoxelWorld,
	const TArray<uint8>& CompressedData)
{
	if (!IsValid(VoxelWorld) || CompressedData.IsEmpty())
	{
		return false;
	}

	TArray64<uint8> UncompressedData;
	if (!FVoxelSerializationUtilities::DecompressData(CompressedData, UncompressedData) ||
		UncompressedData.Num() > MAX_int32)
	{
		return false;
	}

	TArray<uint8> ReaderData;
	ReaderData.Append(UncompressedData.GetData(), static_cast<int32>(UncompressedData.Num()));
	FMemoryReader Reader(ReaderData);
	int32 Version = 0;
	int32 Count = 0;
	Reader << Version;
	Reader << Count;
	if (Reader.IsError() || Version != OwnershipSnapshotVersion || Count < 0)
	{
		return false;
	}

	TMap<FIntVector, int32> TeamByVoxel;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FIntVector VoxelPosition;
		int32 TeamId = INDEX_NONE;
		Reader << VoxelPosition;
		Reader << TeamId;
		if (Reader.IsError())
		{
			return false;
		}
		TeamByVoxel.Add(VoxelPosition, TeamId);
	}

	OwnershipStore.ReplaceSnapshotData(VoxelWorld, MoveTemp(TeamByVoxel));
	return true;
}
