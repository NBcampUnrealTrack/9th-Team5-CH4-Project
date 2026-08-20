#include "DRSnowVolumeStore.h"

namespace
{
	int32 FloorDivide(int32 Value, int32 Divisor)
	{
		return Value >= 0
			? Value / Divisor
			: -((-Value + Divisor - 1) / Divisor);
	}
}

void FDRSnowVolumeStore::CopySnapshotData(
	FDRSnowVolumeSnapshot& OutSnapshot) const
{
	// 직렬화 중 Store가 변하지 않도록 현재 상태를 snapshot DTO로 분리한다.
	OutSnapshot.CellSize = CellSize;
	OutSnapshot.ChunkSize = ChunkSize;
	OutSnapshot.Chunks = Chunks;
}

void FDRSnowVolumeStore::ReplaceSnapshotData(
	FDRSnowVolumeSnapshot&& InSnapshot)
{
	CellSize = FMath::Max(1.f, InSnapshot.CellSize);
	ChunkSize = FMath::Max(1, InSnapshot.ChunkSize);
	Chunks = MoveTemp(InSnapshot.Chunks);

	// ActiveCellIndices는 전송하지 않는 조회용 캐시이므로 복원 뒤 dense cell 데이터에서 다시 만든다.
	for (TPair<FIntVector, FDRSnowVolumeChunk>& ChunkPair : Chunks)
	{
		FDRSnowVolumeChunk& Chunk = ChunkPair.Value;
		Chunk.ActiveCellIndices.Reset();
		for (int32 LocalIndex = 0; LocalIndex < Chunk.Cells.Num(); ++LocalIndex)
		{
			if (Chunk.Cells[LocalIndex].GetTotalAmount() > 0.f)
			{
				Chunk.ActiveCellIndices.Add(LocalIndex);
			}
		}
	}
}

FDRSnowAddResult FDRSnowVolumeStore::AddSnow(
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;

	if (Request.Radius <= 0.f ||
		Request.Amount <= 0.f)
	{
		return Result;
	}

	const FIntVector MinCell =
		WorldToCell(Request.WorldLocation - FVector(Request.Radius));
	const FIntVector MaxCell =
		WorldToCell(Request.WorldLocation + FVector(Request.Radius));
	const float RadiusSquared = FMath::Square(Request.Radius);

	// Voxel edit 결과를 역추적하지 않고, 같은 요청 반경을 snow volume grid에 먼저 기록한다.
	// 이후 렌더/충돌 쪽 VoxelWorld는 이 원본 density를 따라가는 표현 계층으로 취급한다.
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

	return Result;
}

FDRSnowRemoveResult FDRSnowVolumeStore::RemoveSnow(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;

	if (Request.Radius <= 0.f ||
		Request.RequestedAmount <= 0.f)
	{
		return Result;
	}

	const FIntVector MinCell =
		WorldToCell(Request.WorldLocation - FVector(Request.Radius));
	const FIntVector MaxCell =
		WorldToCell(Request.WorldLocation + FVector(Request.Radius));
	const float RadiusSquared = FMath::Square(Request.Radius);

	// AddSnow와 같은 falloff 영역을 사용해 volume density도 실제 흡수 범위에 맞춰 줄인다.
	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				const float RemainingAmount =
					Request.RequestedAmount - Result.RemovedAmount;
				if (RemainingAmount <= 0.f)
				{
					break;
				}

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

				FDRSnowVolumeChunk* Chunk =
					Chunks.Find(CellToChunkOrigin(GlobalCell));
				if (!Chunk)
				{
					continue;
				}

				const float Distance = FMath::Sqrt(DistanceSquared);
				const float Falloff = 1.f - FMath::Clamp(
					Distance / Request.Radius,
					0.f,
					1.f);
				const float CellAmount = FMath::Min(
					RemainingAmount,
					Request.RequestedAmount * Falloff);
				if (CellAmount <= 0.f)
				{
					continue;
				}

				const float RemovedFromCell =
					RemoveSnowFromCell(*Chunk, GlobalCell, CellAmount);
				if (RemovedFromCell <= 0.f)
				{
					continue;
				}

				Result.RemovedAmount += RemovedFromCell;
				++Result.TouchedCellCount;
			}
		}
	}

	return Result;
}

bool FDRSnowVolumeStore::GetSnowCellAtLocation(
	FVector WorldLocation,
	FDRSnowCell& OutCell,
	int32& OutTeamIdA,
	int32& OutTeamIdB) const
{
	// 실패 시에도 out 값을 비워서 호출자가 이전 조회 결과를 실수로 재사용하지 않게 한다.
	const FIntVector Cell = WorldToCell(WorldLocation);
	const FDRSnowVolumeChunk* Chunk = FindChunk(CellToChunkOrigin(Cell));
	if (!Chunk)
	{
		OutCell = FDRSnowCell();
		OutTeamIdA = INDEX_NONE;
		OutTeamIdB = INDEX_NONE;
		return false;
	}

	int32 LocalIndex = INDEX_NONE;
	if (!Chunk->GetLocalIndex(Cell - Chunk->Origin, LocalIndex))
	{
		OutCell = FDRSnowCell();
		OutTeamIdA = INDEX_NONE;
		OutTeamIdB = INDEX_NONE;
		return false;
	}

	OutCell = Chunk->Cells[LocalIndex];
	OutTeamIdA = Chunk->TeamIdA;
	OutTeamIdB = Chunk->TeamIdB;
	return OutCell.GetTotalAmount() > 0.f;
}

int32 FDRSnowVolumeStore::GetDominantTeamAtLocation(
	FVector WorldLocation) const
{
	FDRSnowCell Cell;
	int32 TeamIdA = INDEX_NONE;
	int32 TeamIdB = INDEX_NONE;
	return GetSnowCellAtLocation(WorldLocation, Cell, TeamIdA, TeamIdB)
		? Cell.GetDominantTeamId(TeamIdA, TeamIdB)
		: INDEX_NONE;
}

FDRSnowControlRatio FDRSnowVolumeStore::QuerySnowInBounds(
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

	// 존재하는 chunk와 실제 눈이 있는 cell만 순회한다.
	// 중립 눈은 TotalAmount에는 포함되지만 RatioA/B의 분자에는 포함되지 않는다.
	for (const TPair<FIntVector, FDRSnowVolumeChunk>& ChunkPair : Chunks)
	{
		const FDRSnowVolumeChunk& Chunk = ChunkPair.Value;
		for (const int32 LocalIndex : Chunk.ActiveCellIndices)
		{
			const int32 LocalZ = LocalIndex / (Chunk.Size * Chunk.Size);
			const int32 LocalY = (LocalIndex / Chunk.Size) % Chunk.Size;
			const int32 LocalX = LocalIndex % Chunk.Size;
			const FIntVector GlobalCell = Chunk.Origin + FIntVector(LocalX, LocalY, LocalZ);
			if (GlobalCell.X < MinCell.X || GlobalCell.X > MaxCell.X ||
				GlobalCell.Y < MinCell.Y || GlobalCell.Y > MaxCell.Y ||
				GlobalCell.Z < MinCell.Z || GlobalCell.Z > MaxCell.Z)
			{
				continue;
			}

			const FDRSnowCell& Cell = Chunk.Cells[LocalIndex];
			Ratio.NeutralAmount += Cell.NeutralAmount;
			if (Cell.AmountA > 0.f)
			{
				AddQueriedAmount(Ratio, Chunk.TeamIdA, Cell.AmountA);
			}
			if (Cell.AmountB > 0.f)
			{
				AddQueriedAmount(Ratio, Chunk.TeamIdB, Cell.AmountB);
			}

			++Ratio.SampledCellCount;
		}
	}

	Ratio.TotalAmount = Ratio.NeutralAmount + Ratio.AmountA + Ratio.AmountB;
	if (Ratio.TotalAmount > 0.f)
	{
		Ratio.RatioA = Ratio.AmountA / Ratio.TotalAmount;
		Ratio.RatioB = Ratio.AmountB / Ratio.TotalAmount;
	}

	return Ratio;
}

FIntVector FDRSnowVolumeStore::WorldToCell(const FVector& WorldLocation) const
{
	const float SafeCellSize = FMath::Max(1.f, CellSize);
	return FIntVector(
		FMath::FloorToInt(WorldLocation.X / SafeCellSize),
		FMath::FloorToInt(WorldLocation.Y / SafeCellSize),
		FMath::FloorToInt(WorldLocation.Z / SafeCellSize));
}

FIntVector FDRSnowVolumeStore::CellToChunkOrigin(const FIntVector& Cell) const
{
	const int32 SafeChunkSize = FMath::Max(1, ChunkSize);
	return FIntVector(
		FloorDivide(Cell.X, SafeChunkSize) * SafeChunkSize,
		FloorDivide(Cell.Y, SafeChunkSize) * SafeChunkSize,
		FloorDivide(Cell.Z, SafeChunkSize) * SafeChunkSize);
}

const FDRSnowVolumeChunk* FDRSnowVolumeStore::FindChunk(const FIntVector& ChunkOrigin) const
{
	return Chunks.Find(ChunkOrigin);
}

FDRSnowVolumeChunk& FDRSnowVolumeStore::FindOrCreateChunk(const FIntVector& ChunkOrigin)
{
	if (FDRSnowVolumeChunk* ExistingChunk = Chunks.Find(ChunkOrigin))
	{
		return *ExistingChunk;
	}

	FDRSnowVolumeChunk NewChunk;
	NewChunk.Initialize(ChunkOrigin, FMath::Max(1, ChunkSize), FMath::Max(1.f, CellSize));

	return Chunks.Add(ChunkOrigin, MoveTemp(NewChunk));
}

bool FDRSnowVolumeStore::AddSnowToCell(
	FDRSnowVolumeChunk& Chunk,
	const FIntVector& GlobalCell,
	int32 TeamId,
	float Amount)
{
	if (Amount <= 0.f)
	{
		return false;
	}

	int32 LocalIndex = INDEX_NONE;
	if (!Chunk.GetLocalIndex(GlobalCell - Chunk.Origin, LocalIndex))
	{
		return false;
	}

	FDRSnowCell& Cell = Chunk.Cells[LocalIndex];
	// 중립 눈은 팀 슬롯을 차지하지 않는다.
	// 그래서 기본 지형 눈이 먼저 쌓여 있어도 이후 A/B 팀 기록을 방해하지 않는다.
	if (TeamId == INDEX_NONE)
	{
		Cell.NeutralAmount += Amount;
		Chunk.ActiveCellIndices.Add(LocalIndex);
		return true;
	}

	bool bTeamA = true;
	if (!Chunk.ResolveTeamSlot(TeamId, bTeamA))
	{
		return false;
	}

	if (bTeamA)
	{
		Cell.AmountA += Amount;
	}
	else
	{
		Cell.AmountB += Amount;
	}

	Chunk.ActiveCellIndices.Add(LocalIndex);

	return true;
}

float FDRSnowVolumeStore::RemoveSnowFromCell(
	FDRSnowVolumeChunk& Chunk,
	const FIntVector& GlobalCell,
	float Amount)
{
	if (Amount <= 0.f)
	{
		return 0.f;
	}

	int32 LocalIndex = INDEX_NONE;
	if (!Chunk.GetLocalIndex(GlobalCell - Chunk.Origin, LocalIndex))
	{
		return 0.f;
	}

	FDRSnowCell& Cell = Chunk.Cells[LocalIndex];
	const float TotalAmount = Cell.GetTotalAmount();
	if (TotalAmount <= 0.f)
	{
		return 0.f;
	}

	const float RemovedAmount = FMath::Min(Amount, TotalAmount);
	const float RemoveRatio = RemovedAmount / TotalAmount;

	Cell.NeutralAmount = FMath::Max( 0.f, Cell.NeutralAmount - Cell.NeutralAmount * RemoveRatio);
	Cell.AmountA = FMath::Max(0.f, Cell.AmountA - Cell.AmountA * RemoveRatio);
	Cell.AmountB = FMath::Max(0.f, Cell.AmountB - Cell.AmountB * RemoveRatio);
	if (Cell.GetTotalAmount() <= 0.f)
	{
		Chunk.ActiveCellIndices.Remove(LocalIndex);
	}

	return RemovedAmount;
}

void FDRSnowVolumeStore::AddQueriedAmount(
	FDRSnowControlRatio& InOutRatio,
	int32 TeamId,
	float Amount) const
{
	if (TeamId == INDEX_NONE || Amount <= 0.f)
	{
		return;
	}

	// Query 대상 TeamIdA/B가 비어 있으면 발견 순서대로 채운다.
	// 이미 특정 팀을 넘긴 경우에는 그 팀만 해당 슬롯에 누적된다.
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
