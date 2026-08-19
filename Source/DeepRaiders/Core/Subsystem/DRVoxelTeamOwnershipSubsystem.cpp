#include "DRVoxelTeamOwnershipSubsystem.h"

#include "VoxelWorld.h"

bool UDRVoxelTeamOwnershipSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

void UDRVoxelTeamOwnershipSubsystem::RecordAddedVoxels(
	AVoxelWorld* VoxelWorld,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	int32 TeamId)
{
	if (!IsValid(VoxelWorld))
	{
		return;
	}

	FDRVoxelTeamOwnershipWorldData& WorldData = FindOrCreateWorldData(VoxelWorld);
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		// FVoxelValue에서 양수는 empty, 음수/0은 filled 쪽이다.
		// 이미 채워진 다른 팀 voxel은 이번 Add로 ownership을 덮어쓰지 않는다.
		if (ModifiedValue.OldValue > 0.f &&
			ModifiedValue.NewValue < ModifiedValue.OldValue)
		{
			WorldData.TeamByVoxel.Add(ModifiedValue.Position, TeamId);
		}
	}
}

void UDRVoxelTeamOwnershipSubsystem::RemoveClearedVoxels(
	AVoxelWorld* VoxelWorld,
	const TArray<FModifiedVoxelValue>& ModifiedValues)
{
	if (!IsValid(VoxelWorld))
	{
		return;
	}

	FDRVoxelTeamOwnershipWorldData& WorldData = FindOrCreateWorldData(VoxelWorld);
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		// 흡수 중 값이 조금 올라간 것만으로는 아직 A/B 지형 내부다.
		// 실제 empty 쪽까지 넘어간 voxel만 ownership을 지워야 내부가 기본 땅 재질로 바뀌지 않는다.
		if (ModifiedValue.OldValue <= 0.f && ModifiedValue.NewValue > 0.f)
		{
			WorldData.TeamByVoxel.Remove(ModifiedValue.Position);
		}
	}
}

bool UDRVoxelTeamOwnershipSubsystem::GetTeamAtVoxel(
	AVoxelWorld* VoxelWorld,
	const FIntVector& VoxelPosition,
	int32& OutTeamId) const
{
	OutTeamId = INDEX_NONE;
	const FDRVoxelTeamOwnershipWorldData* WorldData = FindWorldData(VoxelWorld);
	if (!WorldData)
	{
		return false;
	}

	const int32* TeamId = WorldData->TeamByVoxel.Find(VoxelPosition);
	if (!TeamId)
	{
		return false;
	}

	OutTeamId = *TeamId;
	return true;
}

bool UDRVoxelTeamOwnershipSubsystem::GetNearestTeamAtVoxel(
	AVoxelWorld* VoxelWorld,
	const FIntVector& VoxelPosition,
	int32 SearchRadius,
	int32& OutTeamId) const
{
	if (GetTeamAtVoxel(VoxelWorld, VoxelPosition, OutTeamId))
	{
		return true;
	}

	if (SearchRadius <= 0)
	{
		return false;
	}

	const FDRVoxelTeamOwnershipWorldData* WorldData = FindWorldData(VoxelWorld);
	if (!WorldData)
	{
		return false;
	}

	for (int32 Radius = 1; Radius <= SearchRadius; ++Radius)
	{
		for (int32 Z = -Radius; Z <= Radius; ++Z)
		{
			for (int32 Y = -Radius; Y <= Radius; ++Y)
			{
				for (int32 X = -Radius; X <= Radius; ++X)
				{
					if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != Radius)
					{
						continue;
					}

					if (const int32* TeamId = WorldData->TeamByVoxel.Find(VoxelPosition + FIntVector(X, Y, Z)))
					{
						OutTeamId = *TeamId;
						return true;
					}
				}
			}
		}
	}

	OutTeamId = INDEX_NONE;
	return false;
}

FDRVoxelTeamOwnershipWorldData& UDRVoxelTeamOwnershipSubsystem::FindOrCreateWorldData(
	AVoxelWorld* VoxelWorld)
{
	return WorldDataByVoxelWorld.FindOrAdd(TObjectKey<AVoxelWorld>(VoxelWorld));
}

const FDRVoxelTeamOwnershipWorldData* UDRVoxelTeamOwnershipSubsystem::FindWorldData(
	AVoxelWorld* VoxelWorld) const
{
	return IsValid(VoxelWorld)
		? WorldDataByVoxelWorld.Find(TObjectKey<AVoxelWorld>(VoxelWorld))
		: nullptr;
}
