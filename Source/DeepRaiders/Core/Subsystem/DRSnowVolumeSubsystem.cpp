#include "DRSnowVolumeSubsystem.h"

namespace
{
	int32 FloorDivide(int32 Value, int32 Divisor)
	{
		return Value >= 0
			? Value / Divisor
			: -((-Value + Divisor - 1) / Divisor);
	}
}

bool UDRSnowVolumeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

FDRSnowAddResult UDRSnowVolumeSubsystem::AddSnow(
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;

	if (Request.Radius <= 0.f ||
		Request.Amount <= 0.f ||
		Request.Context.TeamId == INDEX_NONE)
	{
		return Result;
	}

	const FIntVector MinCell =
		WorldToCell(Request.WorldLocation - FVector(Request.Radius));
	const FIntVector MaxCell =
		WorldToCell(Request.WorldLocation + FVector(Request.Radius));
	const float RadiusSquared = FMath::Square(Request.Radius);

	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				const FIntVector GlobalCell(X, Y, Z);
				const FVector CellCenter =
					(FVector(
						static_cast<double>(GlobalCell.X),
						static_cast<double>(GlobalCell.Y),
						static_cast<double>(GlobalCell.Z)) +
						FVector(0.5, 0.5, 0.5)) *
					CellSize;
				const float DistanceSquared =
					FVector::DistSquared(CellCenter, Request.WorldLocation);
				if (DistanceSquared > RadiusSquared)
				{
					continue;
				}

				const float Distance = FMath::Sqrt(DistanceSquared);
				const float Falloff = 1.f - FMath::Clamp(
					Distance / Request.Radius,
					0.f,
					1.f);
				const float CellAmount = Request.Amount * Falloff;
				if (CellAmount <= 0.f)
				{
					continue;
				}

				FDRSnowVolumeChunk& Chunk =
					FindOrCreateChunk(CellToChunkOrigin(GlobalCell));
				if (!AddSnowToCell(
					Chunk,
					GlobalCell,
					Request.Context.TeamId,
					CellAmount))
				{
					continue;
				}

				Result.AddedAmount += CellAmount;
				++Result.TouchedCellCount;
			}
		}
	}

	if (Result.AddedAmount > 0.f)
	{
		OnSnowAddedToVolume.Broadcast(Request, Result);
	}

	return Result;
}

FDRSnowControlRatio UDRSnowVolumeSubsystem::QuerySnowInBounds(
	const FBox& WorldBounds,
	int32 TeamIdA,
	int32 TeamIdB) const
{
	FDRSnowControlRatio Ratio;
	Ratio.TeamIdA = TeamIdA;
	Ratio.TeamIdB = TeamIdB;

	if (!WorldBounds.IsValid)
	{
		return Ratio;
	}

	const FIntVector MinCell = WorldToCell(WorldBounds.Min);
	const FIntVector MaxCell = WorldToCell(WorldBounds.Max);

	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				const FIntVector GlobalCell(X, Y, Z);
				const FDRSnowVolumeChunk* Chunk =
					FindChunk(CellToChunkOrigin(GlobalCell));
				if (!Chunk)
				{
					continue;
				}

				int32 LocalIndex = INDEX_NONE;
				if (!Chunk->GetLocalIndex(GlobalCell - Chunk->Origin, LocalIndex))
				{
					continue;
				}

				const FDRSnowCell& Cell = Chunk->Cells[LocalIndex];
				if (Cell.AmountA > 0.f)
				{
					AddQueriedAmount(Ratio, Chunk->TeamIdA, Cell.AmountA);
				}
				if (Cell.AmountB > 0.f)
				{
					AddQueriedAmount(Ratio, Chunk->TeamIdB, Cell.AmountB);
				}

				++Ratio.SampledCellCount;
			}
		}
	}

	Ratio.TotalAmount = Ratio.AmountA + Ratio.AmountB;
	if (Ratio.TotalAmount > 0.f)
	{
		Ratio.RatioA = Ratio.AmountA / Ratio.TotalAmount;
		Ratio.RatioB = Ratio.AmountB / Ratio.TotalAmount;
	}

	return Ratio;
}

FIntVector UDRSnowVolumeSubsystem::WorldToCell(
	const FVector& WorldLocation) const
{
	const float SafeCellSize = FMath::Max(1.f, CellSize);
	return FIntVector(
		FMath::FloorToInt(WorldLocation.X / SafeCellSize),
		FMath::FloorToInt(WorldLocation.Y / SafeCellSize),
		FMath::FloorToInt(WorldLocation.Z / SafeCellSize));
}

FIntVector UDRSnowVolumeSubsystem::CellToChunkOrigin(
	const FIntVector& Cell) const
{
	const int32 SafeChunkSize = FMath::Max(1, ChunkSize);
	return FIntVector(
		FloorDivide(Cell.X, SafeChunkSize) * SafeChunkSize,
		FloorDivide(Cell.Y, SafeChunkSize) * SafeChunkSize,
		FloorDivide(Cell.Z, SafeChunkSize) * SafeChunkSize);
}

const FDRSnowVolumeChunk* UDRSnowVolumeSubsystem::FindChunk(
	const FIntVector& ChunkOrigin) const
{
	return Chunks.Find(ChunkOrigin);
}

FDRSnowVolumeChunk& UDRSnowVolumeSubsystem::FindOrCreateChunk(
	const FIntVector& ChunkOrigin)
{
	if (FDRSnowVolumeChunk* ExistingChunk = Chunks.Find(ChunkOrigin))
	{
		return *ExistingChunk;
	}

	FDRSnowVolumeChunk NewChunk;
	NewChunk.Initialize(
		ChunkOrigin,
		FMath::Max(1, ChunkSize),
		FMath::Max(1.f, CellSize));

	return Chunks.Add(ChunkOrigin, MoveTemp(NewChunk));
}

bool UDRSnowVolumeSubsystem::AddSnowToCell(
	FDRSnowVolumeChunk& Chunk,
	const FIntVector& GlobalCell,
	int32 TeamId,
	float Amount)
{
	bool bTeamA = true;
	if (!Chunk.ResolveTeamSlot(TeamId, bTeamA))
	{
		return false;
	}

	int32 LocalIndex = INDEX_NONE;
	if (!Chunk.GetLocalIndex(GlobalCell - Chunk.Origin, LocalIndex))
	{
		return false;
	}

	FDRSnowCell& Cell = Chunk.Cells[LocalIndex];
	if (bTeamA)
	{
		Cell.AmountA += Amount;
	}
	else
	{
		Cell.AmountB += Amount;
	}

	return true;
}

void UDRSnowVolumeSubsystem::AddQueriedAmount(
	FDRSnowControlRatio& InOutRatio,
	int32 TeamId,
	float Amount) const
{
	if (TeamId == INDEX_NONE || Amount <= 0.f)
	{
		return;
	}

	if (InOutRatio.TeamIdA == INDEX_NONE || InOutRatio.TeamIdA == TeamId)
	{
		InOutRatio.TeamIdA = TeamId;
		InOutRatio.AmountA += Amount;
		return;
	}

	if (InOutRatio.TeamIdB == INDEX_NONE || InOutRatio.TeamIdB == TeamId)
	{
		InOutRatio.TeamIdB = TeamId;
		InOutRatio.AmountB += Amount;
	}
}
