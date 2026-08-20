#include "DRSnowVolumeTypes.h"

void FDRSnowVolumeChunk::Initialize(
	const FIntVector& InOrigin,
	const int32 InSize,
	const float InCellSize)
{
	Origin = InOrigin;
	Size = FMath::Max(1, InSize);
	CellSize = FMath::Max(1.f, InCellSize);
	TeamIds.Reset();
	Cells.SetNum(Size * Size * Size);
	TeamAmounts.Reset();
	ActiveCellIndices.Reset();
}

bool FDRSnowVolumeChunk::GetLocalIndex(
	const FIntVector& LocalCell,
	int32& OutIndex) const
{
	if (LocalCell.X < 0 || LocalCell.Y < 0 || LocalCell.Z < 0 ||
		LocalCell.X >= Size || LocalCell.Y >= Size || LocalCell.Z >= Size)
	{
		return false;
	}

	OutIndex = LocalCell.X + LocalCell.Y * Size + LocalCell.Z * Size * Size;
	return Cells.IsValidIndex(OutIndex);
}

int32 FDRSnowVolumeChunk::FindOrAddTeamSlot(const int32 TeamId)
{
	if (TeamId == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const int32 ExistingSlot = TeamIds.IndexOfByKey(TeamId);
	if (ExistingSlot != INDEX_NONE)
	{
		return ExistingSlot;
	}

	TeamIds.Add(TeamId);
	TeamAmounts.SetNumZeroed(TeamIds.Num() * Cells.Num());
	return TeamIds.Num() - 1;
}

float FDRSnowVolumeChunk::GetTeamAmount(
	const int32 LocalIndex,
	const int32 TeamSlot) const
{
	const int32 AmountIndex = TeamSlot * Cells.Num() + LocalIndex;
	if (!Cells.IsValidIndex(LocalIndex) || !TeamIds.IsValidIndex(TeamSlot) || !TeamAmounts.IsValidIndex(AmountIndex))
	{
		return 0.f;
	}

	return TeamAmounts[AmountIndex];
}

void FDRSnowVolumeChunk::SetTeamAmount(
	const int32 LocalIndex,
	const int32 TeamSlot,
	const float Amount)
{
	const int32 AmountIndex = TeamSlot * Cells.Num() + LocalIndex;
	if (!Cells.IsValidIndex(LocalIndex) || !TeamIds.IsValidIndex(TeamSlot) || !TeamAmounts.IsValidIndex(AmountIndex))
	{
		return;
	}

	TeamAmounts[AmountIndex] = FMath::Max(0.f, Amount);
}

void FDRSnowVolumeChunk::AddTeamAmount(
	const int32 LocalIndex,
	const int32 TeamSlot,
	const float Amount)
{
	SetTeamAmount(LocalIndex, TeamSlot, GetTeamAmount(LocalIndex, TeamSlot) + Amount);
}

float FDRSnowVolumeChunk::GetCellTotalAmount(const int32 LocalIndex) const
{
	if (!Cells.IsValidIndex(LocalIndex))
	{
		return 0.f;
	}

	float TotalAmount = Cells[LocalIndex].NeutralAmount;
	for (int32 TeamSlot = 0; TeamSlot < TeamIds.Num(); ++TeamSlot)
	{
		TotalAmount += GetTeamAmount(LocalIndex, TeamSlot);
	}

	return TotalAmount;
}

int32 FDRSnowVolumeChunk::GetDominantTeamId(const int32 LocalIndex) const
{
	if (!Cells.IsValidIndex(LocalIndex))
	{
		return INDEX_NONE;
	}

	float BestAmount = Cells[LocalIndex].NeutralAmount;
	int32 BestTeamId = INDEX_NONE;
	for (int32 TeamSlot = 0; TeamSlot < TeamIds.Num(); ++TeamSlot)
	{
		const float Amount = GetTeamAmount(LocalIndex, TeamSlot);
		if (Amount > BestAmount)
		{
			BestAmount = Amount;
			BestTeamId = TeamIds[TeamSlot];
		}
	}

	return BestAmount > 0.f ? BestTeamId : INDEX_NONE;
}
