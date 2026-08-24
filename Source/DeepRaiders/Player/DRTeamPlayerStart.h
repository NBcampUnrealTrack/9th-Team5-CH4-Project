#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "DRTeamPlayerStart.generated.h"

/** 지정된 팀 플레이어만 사용하는 시작점이다. */
UCLASS()
class DEEPRAIDERS_API ADRTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team", meta = (ClampMin = "0"))
	int32 TeamId = 0;
};
