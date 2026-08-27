#pragma once

#include "CoreMinimal.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"

class AVoxelWorld;

// DirectionalSurfaceTool이 만든 voxel별 팀 소유권의 원본 저장소다.
class DEEPRAIDERS_API FDRSnowOwnershipStore
{
public:
	// empty -> filled로 바뀐 voxel만 새 팀 소유로 기록한다.
	void RecordAddedVoxels(AVoxelWorld* VoxelWorld, const TArray<FModifiedVoxelValue>& ModifiedValues, int32 TeamId);
	// filled -> empty로 바뀐 voxel의 소유권만 제거한다.
	void RemoveClearedVoxels(AVoxelWorld* VoxelWorld, const TArray<FModifiedVoxelValue>& ModifiedValues);
	// 표면 보간으로 정확한 위치에 기록이 없을 때, 가까운 기록을 제한된 반경에서 찾는다.
	bool GetNearestTeamAtVoxel(AVoxelWorld* VoxelWorld, const FIntVector& VoxelPosition, int32 SearchRadius, int32& OutTeamId) const;
	// 같은 영역의 다수 voxel을 조회할 때 좌표별 결과를 dense cache해 중복 hash lookup을 제거한다.
	void ResolveNearestTeamsAtVoxels(
		AVoxelWorld* VoxelWorld,
		TConstArrayView<FIntVector> VoxelPositions,
		int32 SearchRadius,
		TArray<int32>& OutTeamIds,
		TBitArray<>& OutFoundTeams) const;
	// checkpoint serializer만 사용하는 복사/복원 경계다.
	void CopySnapshotData(AVoxelWorld* VoxelWorld, TMap<FIntVector, int32>& OutTeamByVoxel) const;
	void ReplaceSnapshotData(AVoxelWorld* VoxelWorld, TMap<FIntVector, int32>&& InTeamByVoxel);
	void Reset() { WorldData.Reset(); }

private:
	struct FWorldData
	{
		TMap<FIntVector, int32> TeamByVoxel;
	};
	
	FWorldData& FindOrCreate(AVoxelWorld* VoxelWorld);
	const FWorldData* Find(AVoxelWorld* VoxelWorld) const;
	// PIE와 여러 VoxelWorld가 서로의 local voxel 좌표를 공유하지 않도록 World별로 분리한다.
	TMap<TObjectKey<AVoxelWorld>, FWorldData> WorldData;
};
