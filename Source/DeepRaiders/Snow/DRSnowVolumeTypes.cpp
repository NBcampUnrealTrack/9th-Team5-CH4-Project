#include "DRSnowVolumeTypes.h"

float FDRSnowCell::GetTotalAmount() const
{
	return NeutralAmount + AmountA + AmountB;
}

float FDRSnowCell::GetAmountForTeam(
	int32 TeamId,
	int32 TeamIdA,
	int32 TeamIdB) const
{
	// INDEX_NONE은 "팀 없음"이 아니라 중립 눈 슬롯을 조회하겠다는 의미로 사용한다.
	if (TeamId == INDEX_NONE)
	{
		return NeutralAmount;
	}

	if (TeamId == TeamIdA)
	{
		return AmountA;
	}

	if (TeamId == TeamIdB)
	{
		return AmountB;
	}

	return 0.f;
}

int32 FDRSnowCell::GetDominantTeamId(
	int32 TeamIdA,
	int32 TeamIdB) const
{
	// 중립이 가장 많으면 팀 소유 표면으로 칠하지 않는다.
	float BestAmount = NeutralAmount;
	int32 BestTeamId = INDEX_NONE;

	if (AmountA > BestAmount)
	{
		BestAmount = AmountA;
		BestTeamId = TeamIdA;
	}

	if (AmountB > BestAmount)
	{
		BestAmount = AmountB;
		BestTeamId = TeamIdB;
	}

	return BestAmount > 0.f ? BestTeamId : INDEX_NONE;
}

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

	// Chunk 안의 cell은 TeamId를 직접 저장하지 않고 A/B 슬롯만 저장한다.
	// 그래서 chunk 단위로 실제 TeamId <-> 슬롯 매핑을 관리한다.
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
