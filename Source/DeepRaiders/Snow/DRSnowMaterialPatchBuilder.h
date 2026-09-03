#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowReplicationTypes.h"

struct FModifiedVoxelMaterial;

// 실제로 재질이 바뀐 voxel만 모아 네트워크 전송용 패치로 변환한다.
// 수집 단계에서는 Map/Set으로 중복을 제거하고, Build에서만 정렬된 배열을 만든다.
class FDRSnowMaterialPatchBuilder
{
public:
	void AddChangedMaterials(
		uint8 MaterialIndex,
		const TArray<FModifiedVoxelMaterial>& ModifiedMaterials);

	FDRSnowMaterialPatch Build() const;

private:
	using FLocalIndicesByMaterial = TMap<uint8, TSet<uint16>>;
	TMap<FIntVector, FLocalIndicesByMaterial> Chunks;
};
