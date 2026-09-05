#include "DRSnowReplicationTypes.h"

namespace
{
constexpr uint32 MaxChunkCount = 4096;
constexpr uint32 MaxMaterialSetCountPerChunk = 256;
constexpr uint32 MaxVoxelCountPerSet = 32 * 32 * 32;
constexpr uint32 MaxTotalVoxelCount = 2 * 1024 * 1024;

int32 FloorDivide(const int32 Value, const int32 Divisor)
{
	check(Divisor > 0);
	const int32 Quotient = Value / Divisor;
	const int32 Remainder = Value % Divisor;
	return Remainder < 0 ? Quotient - 1 : Quotient;
}

uint32 EncodeSignedInt(const int32 Value)
{
	return (static_cast<uint32>(Value) << 1) ^ static_cast<uint32>(Value >> 31);
}

int32 DecodeSignedInt(const uint32 Value)
{
	return static_cast<int32>((Value >> 1) ^ -static_cast<int32>(Value & 1));
}

int32 GetPackedUIntSize(uint32 Value)
{
	int32 ByteCount = 1;
	while (Value >= 0x80)
	{
		Value >>= 7;
		++ByteCount;
	}
	return ByteCount;
}

uint32 GetRunLength(const TArray<uint16>& Indices, const uint32 Start)
{
	uint32 End = Start + 1;
	while (End < static_cast<uint32>(Indices.Num()) && Indices[End] == Indices[End - 1] + 1)
	{
		++End;
	}
	return End - Start;
}

bool UseRunEncoding(const TArray<uint16>& Indices, int32& OutIndexBytes)
{
	int32 DeltaBytes = 0;
	int32 RunBytes = 0;
	uint32 Previous = 0;
	for (const uint16 Index : Indices)
	{
		DeltaBytes += GetPackedUIntSize(Index - Previous);
		Previous = Index;
	}
	Previous = 0;
	for (uint32 Start = 0; Start < static_cast<uint32>(Indices.Num());)
	{
		const uint32 Length = GetRunLength(Indices, Start);
		RunBytes += GetPackedUIntSize(Indices[Start] - Previous) + GetPackedUIntSize(Length - 1);
		Previous = Indices[Start + Length - 1];
		Start += Length;
	}
	OutIndexBytes = FMath::Min(DeltaBytes, RunBytes);
	return RunBytes < DeltaBytes;
}

bool SerializeCount(FArchive& Ar, uint32& Value, const uint32 Maximum)
{
	Ar.SerializeIntPacked(Value);
	if (Value > Maximum)
	{
		Ar.SetError();
		return false;
	}
	return !Ar.IsError();
}
}

int32 FDRSnowMaterialPatch::NumVoxels() const
{
	int32 Result = 0;
	for (const FDRSnowMaterialChunkPatch& Chunk : Chunks)
	{
		for (const FDRSnowMaterialIndexSet& MaterialSet : Chunk.MaterialSets)
		{
			Result += MaterialSet.LocalVoxelIndices.Num();
		}
	}
	return Result;
}

int32 FDRSnowMaterialPatch::EstimateSerializedBytes() const
{
	// 각 set의 delta/연속 구간 선택 비트까지 포함한 전체 bit 수를 byte로 올림한다.
	int32 Result = GetPackedUIntSize(Chunks.Num());
	int32 EncodingBits = 0;
	for (const FDRSnowMaterialChunkPatch& Chunk : Chunks)
	{
		Result += GetPackedUIntSize(EncodeSignedInt(Chunk.ChunkCoord.X));
		Result += GetPackedUIntSize(EncodeSignedInt(Chunk.ChunkCoord.Y));
		Result += GetPackedUIntSize(EncodeSignedInt(Chunk.ChunkCoord.Z));
		Result += GetPackedUIntSize(Chunk.MaterialSets.Num());
		for (const FDRSnowMaterialIndexSet& MaterialSet : Chunk.MaterialSets)
		{
			int32 IndexBytes = 0;
			UseRunEncoding(MaterialSet.LocalVoxelIndices, IndexBytes);
			Result += sizeof(uint8) + GetPackedUIntSize(MaterialSet.LocalVoxelIndices.Num()) + IndexBytes;
			++EncodingBits;
		}
	}
	return Result + (EncodingBits + 7) / 8;
}

bool FDRSnowMaterialPatch::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	bOutSuccess = false;

	uint32 ChunkCount = Ar.IsSaving() ? static_cast<uint32>(Chunks.Num()) : 0;
	if (!SerializeCount(Ar, ChunkCount, MaxChunkCount))
	{
		return false;
	}
	if (Ar.IsLoading())
	{
		Chunks.SetNum(ChunkCount);
	}

	uint32 TotalVoxelCount = 0;
	for (FDRSnowMaterialChunkPatch& Chunk : Chunks)
	{
		uint32 EncodedX = Ar.IsSaving() ? EncodeSignedInt(Chunk.ChunkCoord.X) : 0;
		uint32 EncodedY = Ar.IsSaving() ? EncodeSignedInt(Chunk.ChunkCoord.Y) : 0;
		uint32 EncodedZ = Ar.IsSaving() ? EncodeSignedInt(Chunk.ChunkCoord.Z) : 0;
		Ar.SerializeIntPacked(EncodedX);
		Ar.SerializeIntPacked(EncodedY);
		Ar.SerializeIntPacked(EncodedZ);
		if (Ar.IsLoading())
		{
			Chunk.ChunkCoord = FIntVector(
				DecodeSignedInt(EncodedX),
				DecodeSignedInt(EncodedY),
				DecodeSignedInt(EncodedZ));
		}

		uint32 MaterialSetCount = Ar.IsSaving()
			? static_cast<uint32>(Chunk.MaterialSets.Num())
			: 0;
		if (!SerializeCount(Ar, MaterialSetCount, MaxMaterialSetCountPerChunk))
		{
			return false;
		}
		if (Ar.IsLoading())
		{
			Chunk.MaterialSets.SetNum(MaterialSetCount);
		}

		for (FDRSnowMaterialIndexSet& MaterialSet : Chunk.MaterialSets)
		{
			Ar << MaterialSet.MaterialIndex;

			uint32 LocalVoxelCount = Ar.IsSaving()
				? static_cast<uint32>(MaterialSet.LocalVoxelIndices.Num())
				: 0;
			if (!SerializeCount(Ar, LocalVoxelCount, MaxVoxelCountPerSet))
			{
				return false;
			}
			TotalVoxelCount += LocalVoxelCount;
			if (TotalVoxelCount > MaxTotalVoxelCount)
			{
				Ar.SetError();
				return false;
			}

			if (Ar.IsLoading())
			{
				MaterialSet.LocalVoxelIndices.SetNum(LocalVoxelCount);
			}

			uint8 bRuns = 0;
			if (Ar.IsSaving())
			{
				uint32 Previous = 0;
				for (const uint16 LocalIndex : MaterialSet.LocalVoxelIndices)
				{
					if (LocalIndex < Previous || LocalIndex >= MaxVoxelCountPerSet)
					{
						Ar.SetError();
						return false;
					}
					Previous = LocalIndex;
				}
				int32 IndexBytes = 0;
				bRuns = UseRunEncoding(MaterialSet.LocalVoxelIndices, IndexBytes);
			}
			Ar.SerializeBits(&bRuns, 1);

			uint32 PreviousIndex = 0;
			for (uint32 Index = 0; Index < LocalVoxelCount;)
			{
				uint32 Delta = Ar.IsSaving() ? MaterialSet.LocalVoxelIndices[Index] - PreviousIndex : 0;
				Ar.SerializeIntPacked(Delta);
				// 덧셈 전에 검사하여 uint32 overflow를 막는다.
				if (Ar.IsError() || Delta >= MaxVoxelCountPerSet - PreviousIndex)
				{
					Ar.SetError();
					return false;
				}
				const uint32 LocalIndex = PreviousIndex + Delta;
				uint32 LengthMinusOne = 0;
				if (bRuns)
				{
					if (Ar.IsSaving())
					{
						LengthMinusOne = GetRunLength(MaterialSet.LocalVoxelIndices, Index) - 1;
					}
					Ar.SerializeIntPacked(LengthMinusOne);
				}
				if (Ar.IsError() || LengthMinusOne >= LocalVoxelCount - Index ||
					LengthMinusOne >= MaxVoxelCountPerSet - LocalIndex)
				{
					Ar.SetError();
					return false;
				}
				if (Ar.IsLoading())
				{
					for (uint32 Offset = 0; Offset <= LengthMinusOne; ++Offset)
					{
						MaterialSet.LocalVoxelIndices[Index + Offset] = static_cast<uint16>(LocalIndex + Offset);
					}
				}
				PreviousIndex = LocalIndex + LengthMinusOne;
				Index += LengthMinusOne + 1;
			}
		}
	}

	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}

FIntVector DRSnowMaterialPatchUtils::VoxelToChunkCoord(const FIntVector& VoxelPosition)
{
	return FIntVector(
		FloorDivide(VoxelPosition.X, ChunkSize),
		FloorDivide(VoxelPosition.Y, ChunkSize),
		FloorDivide(VoxelPosition.Z, ChunkSize));
}

uint16 DRSnowMaterialPatchUtils::VoxelToLocalIndex(const FIntVector& VoxelPosition)
{
	const FIntVector ChunkCoord = VoxelToChunkCoord(VoxelPosition);
	const FIntVector Local = VoxelPosition - ChunkCoord * ChunkSize;
	check(Local.X >= 0 && Local.X < ChunkSize);
	check(Local.Y >= 0 && Local.Y < ChunkSize);
	check(Local.Z >= 0 && Local.Z < ChunkSize);
	return static_cast<uint16>(Local.X + ChunkSize * (Local.Y + ChunkSize * Local.Z));
}

FIntVector DRSnowMaterialPatchUtils::LocalIndexToVoxel(
	const FIntVector& ChunkCoord,
	const uint16 LocalIndex)
{
	check(LocalIndex < ChunkSize * ChunkSize * ChunkSize);
	const int32 X = LocalIndex % ChunkSize;
	const int32 Y = (LocalIndex / ChunkSize) % ChunkSize;
	const int32 Z = LocalIndex / (ChunkSize * ChunkSize);
	return ChunkCoord * ChunkSize + FIntVector(X, Y, Z);
}
