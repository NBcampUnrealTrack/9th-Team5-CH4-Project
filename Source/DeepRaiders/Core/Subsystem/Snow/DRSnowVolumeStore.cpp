#include "DRSnowVolumeStore.h"

int32 FDRSnowVolumeStore::FloorDivide(const int32 Value, const int32 Divisor)
{
	return Value >= 0 ? Value / Divisor : -((-Value + Divisor - 1) / Divisor);
}

FVector FDRSnowVolumeStore::GetCellCenter(const FIntVector& Cell, const float InCellSize)
{
	return (FVector(Cell.X, Cell.Y, Cell.Z) + FVector(0.5, 0.5, 0.5)) * InCellSize;
}

void FDRSnowVolumeStore::CopySnapshotData(FDRSnowVolumeSnapshot& OutSnapshot) const
{
	OutSnapshot.CellSize = CellSize;
	OutSnapshot.ChunkSize = ChunkSize;
	OutSnapshot.Chunks = Chunks;
}

void FDRSnowVolumeStore::ReplaceSnapshotData(FDRSnowVolumeSnapshot&& InSnapshot)
{
	CellSize = FMath::Max(1.f, InSnapshot.CellSize);
	ChunkSize = FMath::Max(1, InSnapshot.ChunkSize);
	Chunks = MoveTemp(InSnapshot.Chunks);

	// ActiveCellIndices는 전송하지 않는 런타임 캐시라서 복원된 dense data에서 다시 만든다.
	for (TPair<FIntVector, FDRSnowVolumeChunk>& Pair : Chunks)
	{
		FDRSnowVolumeChunk& Chunk = Pair.Value;
		Chunk.ActiveCellIndices.Reset();
		for (int32 LocalIndex = 0; LocalIndex < Chunk.Cells.Num(); ++LocalIndex)
		{
			if (Chunk.GetCellTotalAmount(LocalIndex) > 0.f)
			{
				Chunk.ActiveCellIndices.Add(LocalIndex);
			}
		}
	}
}

FDRSnowAddResult FDRSnowVolumeStore::AddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	Result.TeamId = Request.Context.TeamId;
	const bool bUsesOrientedBox = Request.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool;
	if (Request.Amount <= 0.f ||
		(bUsesOrientedBox && (Request.BoxExtent.X <= 0.f || Request.BoxExtent.Y <= 0.f || Request.BoxExtent.Z <= 0.f)) ||
		(!bUsesOrientedBox && Request.Radius <= 0.f))
	{
		return Result;
	}

	if (bUsesOrientedBox)
	{
		const FTransform BoxTransform(Request.BoxRotation, Request.WorldLocation);
		FBox WorldBounds(ForceInit);
		for (int32 XSign : {-1, 1})
		{
			for (int32 YSign : {-1, 1})
			{
				for (int32 ZSign : {-1, 1})
				{
					WorldBounds += BoxTransform.TransformPosition(FVector(
						Request.BoxExtent.X * XSign,
						Request.BoxExtent.Y * YSign,
						Request.BoxExtent.Z * ZSign));
				}
			}
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
					const FVector LocalPosition = BoxTransform.InverseTransformPosition(GetCellCenter(GlobalCell, CellSize));
					if (FMath::Abs(LocalPosition.X) > Request.BoxExtent.X ||
						FMath::Abs(LocalPosition.Y) > Request.BoxExtent.Y ||
						FMath::Abs(LocalPosition.Z) > Request.BoxExtent.Z)
					{
						continue;
					}

					FDRSnowVolumeChunk& Chunk = FindOrCreateChunk(CellToChunkOrigin(GlobalCell));
					if (AddSnowToCell(Chunk, GlobalCell, Request.Context.TeamId, Request.Amount))
					{
						Result.AddedAmount += Request.Amount;
						++Result.TouchedCellCount;
					}
				}
			}
		}
		return Result;
	}

	const FIntVector MinCell = WorldToCell(Request.WorldLocation - FVector(Request.Radius));
	const FIntVector MaxCell = WorldToCell(Request.WorldLocation + FVector(Request.Radius));
	const float RadiusSquared = FMath::Square(Request.Radius);
	// Voxel 표현과 별개로 같은 반경/falloff를 volume 원본 데이터에 기록한다.
	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				const FIntVector GlobalCell(X, Y, Z);
				const float DistanceSquared = FVector::DistSquared(GetCellCenter(GlobalCell, CellSize), Request.WorldLocation);
				if (DistanceSquared > RadiusSquared)
				{
					continue;
				}

				const float CellAmount = Request.Amount * (1.f - FMath::Clamp(FMath::Sqrt(DistanceSquared) / Request.Radius, 0.f, 1.f));
				if (CellAmount <= 0.f)
				{
					continue;
				}

				FDRSnowVolumeChunk& Chunk = FindOrCreateChunk(CellToChunkOrigin(GlobalCell));
				if (!AddSnowToCell(Chunk, GlobalCell, Request.Context.TeamId, CellAmount))
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

FDRSnowRemoveResult FDRSnowVolumeStore::RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;
	if (Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return Result;
	}

	const FIntVector MinCell = WorldToCell(Request.WorldLocation - FVector(Request.Radius));
	const FIntVector MaxCell = WorldToCell(Request.WorldLocation + FVector(Request.Radius));
	const float RadiusSquared = FMath::Square(Request.Radius);
	// 요청량을 넘지 않는 범위에서 반경 안의 cell을 가까운 순회 순서대로 흡수한다.
	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				const float RemainingAmount = Request.RequestedAmount - Result.RemovedAmount;
				if (RemainingAmount <= 0.f)
				{
					break;
				}

				const FIntVector GlobalCell(X, Y, Z);
				const float DistanceSquared = FVector::DistSquared(GetCellCenter(GlobalCell, CellSize), Request.WorldLocation);
				if (DistanceSquared > RadiusSquared)
				{
					continue;
				}

				FDRSnowVolumeChunk* Chunk = Chunks.Find(CellToChunkOrigin(GlobalCell));
				if (!Chunk)
				{
					continue;
				}

				const float CellAmount = FMath::Min(RemainingAmount, Request.RequestedAmount * (1.f - FMath::Clamp(FMath::Sqrt(DistanceSquared) / Request.Radius, 0.f, 1.f)));
				if (CellAmount <= 0.f)
				{
					continue;
				}

				const float RemovedFromCell = RemoveSnowFromCell(*Chunk, GlobalCell, CellAmount);
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

int32 FDRSnowVolumeStore::GetDominantTeamAtLocation(const FVector WorldLocation) const
{
	const FIntVector Cell = WorldToCell(WorldLocation);
	const FDRSnowVolumeChunk* Chunk = FindChunk(CellToChunkOrigin(Cell));
	if (!Chunk)
	{
		return INDEX_NONE;
	}

	int32 LocalIndex = INDEX_NONE;
	if (!Chunk->GetLocalIndex(Cell - Chunk->Origin, LocalIndex))
	{
		return INDEX_NONE;
	}

	return Chunk->GetDominantTeamId(LocalIndex);
}

FDRSnowControlRatio FDRSnowVolumeStore::QuerySnowInBounds(const FBox& WorldBounds) const
{
	FDRSnowControlRatio Ratio;
	if (!WorldBounds.IsValid)
	{
		return Ratio;
	}

	const FIntVector MinCell = WorldToCell(WorldBounds.Min);
	const FIntVector MaxCell = WorldToCell(WorldBounds.Max);
	for (const TPair<FIntVector, FDRSnowVolumeChunk>& Pair : Chunks)
	{
		const FDRSnowVolumeChunk& Chunk = Pair.Value;
		for (const int32 LocalIndex : Chunk.ActiveCellIndices)
		{
			const FIntVector GlobalCell = Chunk.Origin + FIntVector(LocalIndex % Chunk.Size, (LocalIndex / Chunk.Size) % Chunk.Size, LocalIndex / (Chunk.Size * Chunk.Size));
			if (GlobalCell.X < MinCell.X || GlobalCell.X > MaxCell.X || GlobalCell.Y < MinCell.Y || GlobalCell.Y > MaxCell.Y || GlobalCell.Z < MinCell.Z || GlobalCell.Z > MaxCell.Z)
			{
				continue;
			}

			// TeamIds는 chunk 로컬 palette이므로 Control 결과에서는 실제 TeamId로 다시 합친다.
			Ratio.NeutralAmount += Chunk.Cells[LocalIndex].NeutralAmount;
			for (int32 TeamSlot = 0; TeamSlot < Chunk.TeamIds.Num(); ++TeamSlot)
			{
				AddQueriedAmount(Ratio, Chunk.TeamIds[TeamSlot], Chunk.GetTeamAmount(LocalIndex, TeamSlot));
			}
			++Ratio.SampledCellCount;
		}
	}

	Ratio.TotalAmount = Ratio.NeutralAmount;
	for (FDRSnowTeamAmount& Team : Ratio.Teams)
	{
		Ratio.TotalAmount += Team.Amount;
	}

	if (Ratio.TotalAmount > 0.f)
	{
		for (FDRSnowTeamAmount& Team : Ratio.Teams)
		{
			Team.Ratio = Team.Amount / Ratio.TotalAmount;
		}
	}

	return Ratio;
}

FIntVector FDRSnowVolumeStore::WorldToCell(const FVector& WorldLocation) const
{
	const float SafeCellSize = FMath::Max(1.f, CellSize);
	return FIntVector(FMath::FloorToInt(WorldLocation.X / SafeCellSize), FMath::FloorToInt(WorldLocation.Y / SafeCellSize), FMath::FloorToInt(WorldLocation.Z / SafeCellSize));
}

FIntVector FDRSnowVolumeStore::CellToChunkOrigin(const FIntVector& Cell) const
{
	const int32 SafeChunkSize = FMath::Max(1, ChunkSize);
	return FIntVector(FloorDivide(Cell.X, SafeChunkSize) * SafeChunkSize, FloorDivide(Cell.Y, SafeChunkSize) * SafeChunkSize, FloorDivide(Cell.Z, SafeChunkSize) * SafeChunkSize);
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
	const int32 TeamId,
	const float Amount)
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

	if (TeamId == INDEX_NONE)
	{
		// 중립 눈은 team palette를 차지하지 않는다.
		Chunk.Cells[LocalIndex].NeutralAmount += Amount;
	}
	else
	{
		const int32 TeamSlot = Chunk.FindOrAddTeamSlot(TeamId);
		if (TeamSlot == INDEX_NONE)
		{
			return false;
		}
		Chunk.AddTeamAmount(LocalIndex, TeamSlot, Amount);
	}

	Chunk.ActiveCellIndices.Add(LocalIndex);
	return true;
}

float FDRSnowVolumeStore::RemoveSnowFromCell(
	FDRSnowVolumeChunk& Chunk,
	const FIntVector& GlobalCell,
	const float Amount)
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

	const float TotalAmount = Chunk.GetCellTotalAmount(LocalIndex);
	if (TotalAmount <= 0.f)
	{
		return 0.f;
	}

	// 한 팀만 먼저 깎으면 ownership 비율이 왜곡되므로 모든 성분을 현재 비율대로 줄인다.
	const float RemovedAmount = FMath::Min(Amount, TotalAmount);
	const float RemainingRatio = 1.f - RemovedAmount / TotalAmount;
	Chunk.Cells[LocalIndex].NeutralAmount = FMath::Max(0.f, Chunk.Cells[LocalIndex].NeutralAmount * RemainingRatio);
	for (int32 TeamSlot = 0; TeamSlot < Chunk.TeamIds.Num(); ++TeamSlot)
	{
		Chunk.SetTeamAmount(LocalIndex, TeamSlot, Chunk.GetTeamAmount(LocalIndex, TeamSlot) * RemainingRatio);
	}

	if (Chunk.GetCellTotalAmount(LocalIndex) <= 0.f)
	{
		Chunk.ActiveCellIndices.Remove(LocalIndex);
	}

	return RemovedAmount;
}

void FDRSnowVolumeStore::AddQueriedAmount(
	FDRSnowControlRatio& InOutRatio,
	const int32 TeamId,
	const float Amount) const
{
	if (TeamId == INDEX_NONE || Amount <= 0.f)
	{
		return;
	}

	FDRSnowTeamAmount* Team = InOutRatio.Teams.FindByPredicate(
		[TeamId](const FDRSnowTeamAmount& Entry)
		{
			return Entry.TeamId == TeamId;
		});
	if (!Team)
	{
		// 서로 다른 chunk의 같은 TeamId를 하나의 control entry로 합친다.
		Team = &InOutRatio.Teams.AddDefaulted_GetRef();
		Team->TeamId = TeamId;
	}

	Team->Amount += Amount;
}
