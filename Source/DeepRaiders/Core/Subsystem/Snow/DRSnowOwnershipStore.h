#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowReplicationTypes.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"

class AVoxelWorld;

// 서버가 눈 voxel별 최종 MaterialIndex를 32^3 청크 단위로 보관하는 원본 저장소다.
// TeamId별 양은 VolumeStore가 담당하고, 이 저장소는 표면 표현 복원에 필요한 값만 가진다.
class DEEPRAIDERS_API FDRSnowOwnershipStore
{
public:
	// empty -> filled로 바뀐 voxel만 새 MaterialIndex로 기록한다.
	void RecordAddedVoxels(
		AVoxelWorld* VoxelWorld,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		uint8 MaterialIndex);
	// filled -> empty로 바뀐 voxel의 MaterialIndex만 제거한다.
	void RemoveClearedVoxels(AVoxelWorld* VoxelWorld, const TArray<FModifiedVoxelValue>& ModifiedValues);
	// 표면 보간으로 정확한 위치에 기록이 없을 때, 가까운 기록을 제한된 반경에서 찾는다.
	bool GetNearestMaterialIndexAtVoxel(
		AVoxelWorld* VoxelWorld,
		const FIntVector& VoxelPosition,
		int32 SearchRadius,
		uint8& OutMaterialIndex) const;
	// 같은 영역의 다수 voxel을 조회할 때 좌표별 결과를 dense cache해 중복 hash lookup을 제거한다.
	void ResolveNearestMaterialIndicesAtVoxels(
		AVoxelWorld* VoxelWorld,
		TConstArrayView<FIntVector> VoxelPositions,
		int32 SearchRadius,
		TArray<uint8>& OutMaterialIndices,
		TBitArray<>& OutFoundMaterials) const;
	void Reset() { WorldData.Reset(); }

private:
	struct FChunkData
	{
		// key는 32^3 청크 내부의 packed voxel index다.
		TMap<uint16, uint8> MaterialByLocalVoxel;
	};

	struct FWorldData
	{
		TMap<FIntVector, FChunkData> Chunks;
	};

	static bool FindExactMaterial(
		const FWorldData& Data,
		const FIntVector& VoxelPosition,
		uint8& OutMaterialIndex);
	FWorldData& FindOrCreate(AVoxelWorld* VoxelWorld);
	const FWorldData* Find(AVoxelWorld* VoxelWorld) const;
	// PIE와 여러 VoxelWorld가 서로의 local voxel 좌표를 공유하지 않도록 World별로 분리한다.
	TMap<TObjectKey<AVoxelWorld>, FWorldData> WorldData;
};
