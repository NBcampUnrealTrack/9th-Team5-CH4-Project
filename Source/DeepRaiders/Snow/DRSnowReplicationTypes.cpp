#include "DRSnowReplicationTypes.h"

namespace
{
int32 FloorDivide(const int32 Value, const int32 Divisor)
{
	check(Divisor > 0);
	const int32 Quotient = Value / Divisor;
	const int32 Remainder = Value % Divisor;
	return Remainder < 0 ? Quotient - 1 : Quotient;
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
	// TArray count, ChunkCoord, MaterialSet count, MaterialIndex, LocalIndex count와 payload의 하한 추정치다.
	int32 Result = sizeof(int32);
	for (const FDRSnowMaterialChunkPatch& Chunk : Chunks)
	{
		Result += sizeof(FIntVector) + sizeof(int32);
		for (const FDRSnowMaterialIndexSet& MaterialSet : Chunk.MaterialSets)
		{
			Result += sizeof(uint8) + sizeof(int32) + sizeof(uint16) * MaterialSet.LocalVoxelIndices.Num();
		}
	}
	return Result;
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
