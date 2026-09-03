#include "DRSnowOwnershipStore.h"

#include "ProfilingDebugging/CountersTrace.h"
#include "VoxelWorld.h"

void FDRSnowOwnershipStore::RecordAddedVoxels(
	AVoxelWorld* World,
	const TArray<FModifiedVoxelValue>& Values,
	const uint8 MaterialIndex)
{
	if (!IsValid(World))
	{
		return;
	}
	FWorldData& Data = FindOrCreate(World);
	for (const FModifiedVoxelValue& Value : Values)
	{
		if (Value.OldValue > 0.f && Value.NewValue < Value.OldValue)
		{
			const FIntVector ChunkCoord = DRSnowMaterialPatchUtils::VoxelToChunkCoord(Value.Position);
			FChunkData& Chunk = Data.Chunks.FindOrAdd(ChunkCoord);
			Chunk.MaterialByLocalVoxel.Add(
				DRSnowMaterialPatchUtils::VoxelToLocalIndex(Value.Position),
				MaterialIndex);
		}
	}
}

void FDRSnowOwnershipStore::RemoveClearedVoxels(AVoxelWorld* World, const TArray<FModifiedVoxelValue>& Values)
{
	if (!IsValid(World))
	{
		return;
	}
	FWorldData& Data = FindOrCreate(World);
	for (const FModifiedVoxelValue& Value : Values)
	{
		if (Value.OldValue <= 0.f && Value.NewValue > 0.f)
		{
			const FIntVector ChunkCoord = DRSnowMaterialPatchUtils::VoxelToChunkCoord(Value.Position);
			FChunkData* Chunk = Data.Chunks.Find(ChunkCoord);
			if (!Chunk)
			{
				continue;
			}

			Chunk->MaterialByLocalVoxel.Remove(
				DRSnowMaterialPatchUtils::VoxelToLocalIndex(Value.Position));
			if (Chunk->MaterialByLocalVoxel.IsEmpty())
			{
				Data.Chunks.Remove(ChunkCoord);
			}
		}
	}
}

bool FDRSnowOwnershipStore::FindExactMaterial(
	const FWorldData& Data,
	const FIntVector& Position,
	uint8& OutMaterialIndex)
{
	const FIntVector ChunkCoord = DRSnowMaterialPatchUtils::VoxelToChunkCoord(Position);
	const FChunkData* Chunk = Data.Chunks.Find(ChunkCoord);
	if (!Chunk)
	{
		return false;
	}

	if (const uint8* MaterialIndex = Chunk->MaterialByLocalVoxel.Find(
		DRSnowMaterialPatchUtils::VoxelToLocalIndex(Position)))
	{
		OutMaterialIndex = *MaterialIndex;
		return true;
	}
	return false;
}

bool FDRSnowOwnershipStore::GetNearestMaterialIndexAtVoxel(
	AVoxelWorld* World,
	const FIntVector& Position,
	const int32 Radius,
	uint8& OutMaterialIndex) const
{
	OutMaterialIndex = 0;
	const FWorldData* Data = Find(World);
	if (!Data)
	{
		return false;
	}
	if (FindExactMaterial(*Data, Position, OutMaterialIndex))
	{
		return true;
	}
	for (int32 CurrentRadius = 1; CurrentRadius <= Radius; ++CurrentRadius)
	{
		for (int32 Z = -CurrentRadius; Z <= CurrentRadius; ++Z)
		{
			for (int32 Y = -CurrentRadius; Y <= CurrentRadius; ++Y)
			{
				for (int32 X = -CurrentRadius; X <= CurrentRadius; ++X)
				{
					if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != CurrentRadius)
					{
						continue;
					}
					if (FindExactMaterial(
						*Data,
						Position + FIntVector(X, Y, Z),
						OutMaterialIndex))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void FDRSnowOwnershipStore::ResolveNearestMaterialIndicesAtVoxels(
	AVoxelWorld* World,
	const TConstArrayView<FIntVector> Positions,
	const int32 Radius,
	TArray<uint8>& OutMaterialIndices,
	TBitArray<>& OutFoundMaterials) const
{
	OutMaterialIndices.Init(0, Positions.Num());
	OutFoundMaterials.Init(false, Positions.Num());
	const FWorldData* Data = Find(World);
	if (!Data || Positions.IsEmpty())
	{
		return;
	}

	const int32 ClampedRadius = FMath::Max(0, Radius);
	FIntVector CacheMin = Positions[0] - FIntVector(ClampedRadius);
	FIntVector CacheMax = Positions[0] + FIntVector(ClampedRadius);
	for (const FIntVector& Position : Positions)
	{
		CacheMin.X = FMath::Min(CacheMin.X, Position.X - ClampedRadius);
		CacheMin.Y = FMath::Min(CacheMin.Y, Position.Y - ClampedRadius);
		CacheMin.Z = FMath::Min(CacheMin.Z, Position.Z - ClampedRadius);
		CacheMax.X = FMath::Max(CacheMax.X, Position.X + ClampedRadius);
		CacheMax.Y = FMath::Max(CacheMax.Y, Position.Y + ClampedRadius);
		CacheMax.Z = FMath::Max(CacheMax.Z, Position.Z + ClampedRadius);
	}

	const FIntVector CacheSize = CacheMax - CacheMin + FIntVector(1);
	const int64 CacheVoxelCount64 =
		static_cast<int64>(CacheSize.X) * CacheSize.Y * CacheSize.Z;
	if (CacheVoxelCount64 <= 0 || CacheVoxelCount64 > MAX_int32)
	{
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			OutFoundMaterials[Index] = GetNearestMaterialIndexAtVoxel(
				World,
				Positions[Index],
				ClampedRadius,
				OutMaterialIndices[Index]);
		}
		return;
	}

	const int32 CacheVoxelCount = static_cast<int32>(CacheVoxelCount64);
	TArray<uint8> CachedMaterialIndices;
	CachedMaterialIndices.Init(0, CacheVoxelCount);
	TBitArray<> CachedPositions(false, CacheVoxelCount);
	TBitArray<> CachedFoundMaterials(false, CacheVoxelCount);
	int32 ExactMapLookupCount = 0;
	int32 CacheReuseCount = 0;

	auto FindCachedMaterial = [Data, CacheMin, CacheSize, &CachedMaterialIndices, &CachedPositions,
		&CachedFoundMaterials, &ExactMapLookupCount, &CacheReuseCount](
		const FIntVector& Position,
		uint8& OutMaterialIndex)
	{
		const FIntVector LocalPosition = Position - CacheMin;
		const int32 CacheIndex =
			LocalPosition.X + CacheSize.X * (LocalPosition.Y + CacheSize.Y * LocalPosition.Z);
		if (!CachedPositions[CacheIndex])
		{
			CachedPositions[CacheIndex] = true;
			++ExactMapLookupCount;
			uint8 MaterialIndex = 0;
			if (FindExactMaterial(*Data, Position, MaterialIndex))
			{
				CachedMaterialIndices[CacheIndex] = MaterialIndex;
				CachedFoundMaterials[CacheIndex] = true;
			}
		}
		else
		{
			++CacheReuseCount;
		}

		OutMaterialIndex = CachedMaterialIndices[CacheIndex];
		return CachedFoundMaterials[CacheIndex];
	};

	for (int32 PositionIndex = 0; PositionIndex < Positions.Num(); ++PositionIndex)
	{
		const FIntVector& Position = Positions[PositionIndex];
		if (FindCachedMaterial(Position, OutMaterialIndices[PositionIndex]))
		{
			OutFoundMaterials[PositionIndex] = true;
			continue;
		}

		for (int32 CurrentRadius = 1; CurrentRadius <= ClampedRadius && !OutFoundMaterials[PositionIndex]; ++CurrentRadius)
		{
			for (int32 Z = -CurrentRadius; Z <= CurrentRadius && !OutFoundMaterials[PositionIndex]; ++Z)
			{
				for (int32 Y = -CurrentRadius; Y <= CurrentRadius && !OutFoundMaterials[PositionIndex]; ++Y)
				{
					for (int32 X = -CurrentRadius; X <= CurrentRadius; ++X)
					{
						if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != CurrentRadius)
						{
							continue;
						}
						if (FindCachedMaterial(
							Position + FIntVector(X, Y, Z),
							OutMaterialIndices[PositionIndex]))
						{
							OutFoundMaterials[PositionIndex] = true;
							break;
						}
					}
				}
			}
		}
	}

	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Ownership/ExactMapLookups"), ExactMapLookupCount);
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Ownership/CacheReuses"), CacheReuseCount);
}

FDRSnowOwnershipStore::FWorldData& FDRSnowOwnershipStore::FindOrCreate(AVoxelWorld* World)
{
	return WorldData.FindOrAdd(TObjectKey<AVoxelWorld>(World));
}

const FDRSnowOwnershipStore::FWorldData* FDRSnowOwnershipStore::Find(AVoxelWorld* World) const
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	return WorldData.Find(TObjectKey<AVoxelWorld>(World));
}
