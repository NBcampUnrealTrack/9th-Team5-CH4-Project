#include "DRSnowOwnershipStore.h"

#include "ProfilingDebugging/CountersTrace.h"
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

void FDRSnowOwnershipStore::ResolveNearestTeamsAtVoxels(
	AVoxelWorld* World,
	const TConstArrayView<FIntVector> Positions,
	const int32 Radius,
	TArray<int32>& OutTeamIds,
	TBitArray<>& OutFoundTeams) const
{
	OutTeamIds.Init(INDEX_NONE, Positions.Num());
	OutFoundTeams.Init(false, Positions.Num());
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
			OutFoundTeams[Index] = GetNearestTeamAtVoxel(
				World,
				Positions[Index],
				ClampedRadius,
				OutTeamIds[Index]);
		}
		return;
	}

	const int32 CacheVoxelCount = static_cast<int32>(CacheVoxelCount64);
	TArray<int32> CachedTeamIds;
	CachedTeamIds.Init(INDEX_NONE, CacheVoxelCount);
	TBitArray<> CachedPositions(false, CacheVoxelCount);
	TBitArray<> CachedFoundTeams(false, CacheVoxelCount);
	int32 ExactMapLookupCount = 0;
	int32 CacheReuseCount = 0;

	auto FindExactTeam = [Data, CacheMin, CacheSize, &CachedTeamIds, &CachedPositions,
		&CachedFoundTeams, &ExactMapLookupCount, &CacheReuseCount](
		const FIntVector& Position,
		int32& OutTeamId)
	{
		const FIntVector LocalPosition = Position - CacheMin;
		const int32 CacheIndex =
			LocalPosition.X + CacheSize.X * (LocalPosition.Y + CacheSize.Y * LocalPosition.Z);
		if (!CachedPositions[CacheIndex])
		{
			CachedPositions[CacheIndex] = true;
			++ExactMapLookupCount;
			if (const int32* TeamId = Data->TeamByVoxel.Find(Position))
			{
				CachedTeamIds[CacheIndex] = *TeamId;
				CachedFoundTeams[CacheIndex] = true;
			}
		}
		else
		{
			++CacheReuseCount;
		}

		OutTeamId = CachedTeamIds[CacheIndex];
		return CachedFoundTeams[CacheIndex];
	};

	for (int32 PositionIndex = 0; PositionIndex < Positions.Num(); ++PositionIndex)
	{
		const FIntVector& Position = Positions[PositionIndex];
		if (FindExactTeam(Position, OutTeamIds[PositionIndex]))
		{
			OutFoundTeams[PositionIndex] = true;
			continue;
		}

		for (int32 CurrentRadius = 1; CurrentRadius <= ClampedRadius && !OutFoundTeams[PositionIndex]; ++CurrentRadius)
		{
			for (int32 Z = -CurrentRadius; Z <= CurrentRadius && !OutFoundTeams[PositionIndex]; ++Z)
			{
				for (int32 Y = -CurrentRadius; Y <= CurrentRadius && !OutFoundTeams[PositionIndex]; ++Y)
				{
					for (int32 X = -CurrentRadius; X <= CurrentRadius; ++X)
					{
						if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != CurrentRadius)
						{
							continue;
						}
						if (FindExactTeam(Position + FIntVector(X, Y, Z), OutTeamIds[PositionIndex]))
						{
							OutFoundTeams[PositionIndex] = true;
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
