#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRMiningGameModeBase.generated.h"

// 채굴 테스트/플레이용 GameState를 사용하는 GameMode이다.
UCLASS()
class DEEPRAIDERS_API ADRMiningGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRMiningGameModeBase();

	virtual void PostLogin(APlayerController* NewPlayer) override;
};
