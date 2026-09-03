#pragma once

#include "CoreMinimal.h"
#include "DRSnowReplicationTypes.generated.h"

class UPackageMap;

USTRUCT()
struct DEEPRAIDERS_API FDRSnowMaterialIndexSet
{
	GENERATED_BODY()

	// 0=Neutral, 1=Team0, 2=Team1
	UPROPERTY()
	uint8 MaterialIndex = 0;

	// 32³ 청크 내부 Voxel 위치. 패치 빌더가 오름차순/중복 제거를 보장한다.
	UPROPERTY()
	TArray<uint16> LocalVoxelIndices;
};

USTRUCT()
struct DEEPRAIDERS_API FDRSnowMaterialChunkPatch
{
	GENERATED_BODY()

	// VoxelWorld 로컬 좌표 기준 청크 좌표
	UPROPERTY()
	FIntVector ChunkCoord = FIntVector::ZeroValue;

	// 같은 MaterialIndex를 적용할 Voxel 묶음
	UPROPERTY()
	TArray<FDRSnowMaterialIndexSet> MaterialSets;
};

USTRUCT()
struct DEEPRAIDERS_API FDRSnowMaterialPatch
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDRSnowMaterialChunkPatch> Chunks;

	bool IsEmpty() const { return Chunks.IsEmpty(); }
	int32 NumVoxels() const;
	int32 EstimateSerializedBytes() const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FDRSnowMaterialPatch> : TStructOpsTypeTraitsBase2<FDRSnowMaterialPatch>
{
	enum
	{
		WithNetSerializer = true
	};
};

namespace DRSnowMaterialPatchUtils
{
	inline constexpr int32 ChunkSize = 32;

	DEEPRAIDERS_API FIntVector VoxelToChunkCoord(const FIntVector& VoxelPosition);
	DEEPRAIDERS_API uint16 VoxelToLocalIndex(const FIntVector& VoxelPosition);
	DEEPRAIDERS_API FIntVector LocalIndexToVoxel(const FIntVector& ChunkCoord, uint16 LocalIndex);
}
