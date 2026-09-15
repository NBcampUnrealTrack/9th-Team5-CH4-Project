#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

struct FDRSnowVolumeSnapshot
{
	float CellSize = 20.f;
	int32 ChunkSize = 32;
	TMap<FIntVector, FDRSnowVolumeChunk> Chunks;
};

class DEEPRAIDERS_API FDRSnowVolumeStore
{
public:
	FDRSnowAddResult AddSnow(const FDRSnowSurfaceAddRequest& Request);
	FDRSnowRemoveResult RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request);
	int32 GetDominantTeamAtLocation(FVector WorldLocation) const;
	FDRSnowControlRatio QuerySnowInBounds(const FBox& WorldBounds) const;
	void CopySnapshotData(FDRSnowVolumeSnapshot& OutSnapshot) const;
	void ReplaceSnapshotData(FDRSnowVolumeSnapshot&& InSnapshot);
	void Reset() { Chunks.Reset(); }

protected:
	static int32 FloorDivide(int32 Value, int32 Divisor);
	static FVector GetCellCenter(const FIntVector& Cell, float CellSize);
	FIntVector WorldToCell(const FVector& WorldLocation) const;
	FIntVector CellToChunkOrigin(const FIntVector& Cell) const;
	const FDRSnowVolumeChunk* FindChunk(const FIntVector& ChunkOrigin) const;
	FDRSnowVolumeChunk& FindOrCreateChunk(const FIntVector& ChunkOrigin);
	bool AddSnowToCell(FDRSnowVolumeChunk& Chunk, const FIntVector& GlobalCell, int32 TeamId, float Amount);
	float RemoveSnowFromCell(FDRSnowVolumeChunk& Chunk, const FIntVector& GlobalCell, float Amount);
	void AddQueriedAmount(FDRSnowControlRatio& InOutRatio, int32 TeamId, float Amount) const;

	float CellSize = 20.f;
	int32 ChunkSize = 32;
	TMap<FIntVector, FDRSnowVolumeChunk> Chunks;
};
