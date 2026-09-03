#pragma once

#include "CoreMinimal.h"

// TeamId는 게임 규칙용 식별자이고 MaterialIndex는 Voxel 표현용 식별자다.
// 현재 프로젝트의 고정 규칙(Neutral=0, Team N=N+1)을 한곳에서 관리한다.
namespace DRSnowMaterialMapping
{
	FORCEINLINE uint8 TeamToMaterialIndex(const int32 TeamId)
	{
		if (TeamId == INDEX_NONE)
		{
			return 0;
		}

		ensureMsgf(
			TeamId >= 0 && TeamId <= MAX_uint8 - 1,
			TEXT("Snow TeamId %d cannot be represented by a uint8 MaterialIndex"),
			TeamId);
		return static_cast<uint8>(
			FMath::Clamp(TeamId, 0, static_cast<int32>(MAX_uint8) - 1) + 1);
	}

	FORCEINLINE int32 MaterialIndexToTeamId(const uint8 MaterialIndex)
	{
		return MaterialIndex == 0
			? INDEX_NONE
			: static_cast<int32>(MaterialIndex) - 1;
	}
}
