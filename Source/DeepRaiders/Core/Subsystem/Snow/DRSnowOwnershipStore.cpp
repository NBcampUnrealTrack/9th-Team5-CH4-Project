#include "DRSnowOwnershipStore.h"

#include "VoxelWorld.h"

void FDRSnowOwnershipStore::RecordAddedVoxels(AVoxelWorld* World, const TArray<FModifiedVoxelValue>& Values, int32 TeamId)
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
			Data.TeamByVoxel.Add(Value.Position, TeamId);
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
			Data.TeamByVoxel.Remove(Value.Position);
		}
	}
}

bool FDRSnowOwnershipStore::GetNearestTeamAtVoxel(AVoxelWorld* World, const FIntVector& Position, int32 Radius, int32& OutTeamId) const
{
	OutTeamId = INDEX_NONE;
	const FWorldData* Data = Find(World);
	if (!Data)
	{
		return false;
	}
	if (const int32* Team = Data->TeamByVoxel.Find(Position))
	{
		OutTeamId = *Team;
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
					if (const int32* Team = Data->TeamByVoxel.Find(Position + FIntVector(X, Y, Z)))
					{
						OutTeamId = *Team;
						return true;
					}
				}
			}
		}
	}
	return false;
}

void FDRSnowOwnershipStore::CopySnapshotData(AVoxelWorld* World, TMap<FIntVector, int32>& Out) const
{
	Out.Reset();
	if (const FWorldData* Data = Find(World))
	{
		Out = Data->TeamByVoxel;
	}
}

void FDRSnowOwnershipStore::ReplaceSnapshotData(AVoxelWorld* World, TMap<FIntVector, int32>&& In)
{
	if (IsValid(World))
	{
		FindOrCreate(World).TeamByVoxel = MoveTemp(In);
	}
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
