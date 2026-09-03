#include "DRSnowMaterialPatchBuilder.h"

#include "VoxelMaterial.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"

namespace
{
bool HasMaterialChanged(const FModifiedVoxelMaterial& ModifiedMaterial)
{
	// FVoxelMaterial은 Voxel Plugin에서 bytewise-comparable 타입으로 선언되어 있다.
	return FMemory::Memcmp(
		&ModifiedMaterial.OldMaterial,
		&ModifiedMaterial.NewMaterial,
		sizeof(FVoxelMaterial)) != 0;
}

bool IsChunkBefore(const FIntVector& A, const FIntVector& B)
{
	if (A.X != B.X)
	{
		return A.X < B.X;
	}
	if (A.Y != B.Y)
	{
		return A.Y < B.Y;
	}
	return A.Z < B.Z;
}
}

void FDRSnowMaterialPatchBuilder::AddChangedMaterials(
	const uint8 MaterialIndex,
	const TArray<FModifiedVoxelMaterial>& ModifiedMaterials)
{
	for (const FModifiedVoxelMaterial& ModifiedMaterial : ModifiedMaterials)
	{
		if (!HasMaterialChanged(ModifiedMaterial))
		{
			continue;
		}

		const FIntVector ChunkCoord =
			DRSnowMaterialPatchUtils::VoxelToChunkCoord(ModifiedMaterial.Position);
		Chunks.FindOrAdd(ChunkCoord)
			.FindOrAdd(MaterialIndex)
			.Add(DRSnowMaterialPatchUtils::VoxelToLocalIndex(ModifiedMaterial.Position));
	}
}

FDRSnowMaterialPatch FDRSnowMaterialPatchBuilder::Build() const
{
	FDRSnowMaterialPatch Patch;
	Patch.Chunks.Reserve(Chunks.Num());
	for (const TPair<FIntVector, FLocalIndicesByMaterial>& Chunk : Chunks)
	{
		FDRSnowMaterialChunkPatch& ChunkPatch = Patch.Chunks.AddDefaulted_GetRef();
		ChunkPatch.ChunkCoord = Chunk.Key;
	}
	Patch.Chunks.Sort([](const FDRSnowMaterialChunkPatch& A, const FDRSnowMaterialChunkPatch& B)
	{
		return IsChunkBefore(A.ChunkCoord, B.ChunkCoord);
	});

	for (FDRSnowMaterialChunkPatch& ChunkPatch : Patch.Chunks)
	{
		const FLocalIndicesByMaterial& MaterialMap = Chunks.FindChecked(ChunkPatch.ChunkCoord);

		TArray<uint8> MaterialIndices;
		MaterialMap.GenerateKeyArray(MaterialIndices);
		MaterialIndices.Sort();
		ChunkPatch.MaterialSets.Reserve(MaterialIndices.Num());

		for (const uint8 MaterialIndex : MaterialIndices)
		{
			FDRSnowMaterialIndexSet& MaterialSet = ChunkPatch.MaterialSets.AddDefaulted_GetRef();
			MaterialSet.MaterialIndex = MaterialIndex;
			MaterialSet.LocalVoxelIndices = MaterialMap.FindChecked(MaterialIndex).Array();
			MaterialSet.LocalVoxelIndices.Sort();
		}
	}

	return Patch;
}
