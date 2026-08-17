#include "DRSnowVolumeTypes.h"

void FDRSnowVolumeChunk::Initialize(
	const FIntVector& InOrigin,
	int32 InSize,
	float InCellSize)
{
	Origin = InOrigin;
	Size = FMath::Max(1, InSize);
	CellSize = FMath::Max(1.f, InCellSize);
	Cells.SetNum(Size * Size * Size);
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

	OutIndex = LocalCell.X +
		LocalCell.Y * Size +
		LocalCell.Z * Size * Size;
	return Cells.IsValidIndex(OutIndex);
}

bool FDRSnowVolumeChunk::ResolveTeamSlot(int32 TeamId, bool& bOutTeamA)
{
	if (TeamId == INDEX_NONE)
	{
		return false;
	}

	if (TeamIdA == TeamId)
	{
		bOutTeamA = true;
		return true;
	}

	if (TeamIdB == TeamId)
	{
		bOutTeamA = false;
		return true;
	}

	if (TeamIdA == INDEX_NONE)
	{
		TeamIdA = TeamId;
		bOutTeamA = true;
		return true;
	}

	if (TeamIdB == INDEX_NONE)
	{
		TeamIdB = TeamId;
		bOutTeamA = false;
		return true;
	}

	return false;
}
