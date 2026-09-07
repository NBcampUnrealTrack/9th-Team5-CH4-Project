#pragma once

#include "CoreMinimal.h"
#include "DRGameFlowState.generated.h"

// 경기 내부의 시간제 페이즈와 구분되는 전체 경기 상태다.
UENUM(BlueprintType)
enum class EDRGameFlowState : uint8
{
	WaitingForPlayers,
	Countdown,
	Loading,
	Playing,
	Results
};
