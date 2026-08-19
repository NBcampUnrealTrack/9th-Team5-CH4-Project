#pragma once

#include "CoreMinimal.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"

class AVoxelWorld;

// DirectionalSurfaceTool이 만든 voxel별 팀 소유권의 원본 저장소다.
class DEEPRAIDERS_API FDRSnowOwnershipStore
{
public:
	void RecordAddedVoxels(AVoxelWorld* VoxelWorld, const TArray<FModifiedVoxelValue>& ModifiedValues, int32 TeamId);
	void RemoveClearedVoxels(AVoxelWorld* VoxelWorld, const TArray<FModifiedVoxelValue>& ModifiedValues);
	bool GetNearestTeamAtVoxel(AVoxelWorld* VoxelWorld, const FIntVector& VoxelPosition, int32 SearchRadius, int32& OutTeamId) const;
	void CopySnapshotData(AVoxelWorld* VoxelWorld, TMap<FIntVector, int32>& OutTeamByVoxel) const;
	void ReplaceSnapshotData(AVoxelWorld* VoxelWorld, TMap<FIntVector, int32>&& InTeamByVoxel);

private:
	struct FWorldData
	{
		TMap<FIntVector, int32> TeamByVoxel;
	};
	
	FWorldData& FindOrCreate(AVoxelWorld* VoxelWorld);
	const FWorldData* Find(AVoxelWorld* VoxelWorld) const;
	TMap<TObjectKey<AVoxelWorld>, FWorldData> WorldData;
};
