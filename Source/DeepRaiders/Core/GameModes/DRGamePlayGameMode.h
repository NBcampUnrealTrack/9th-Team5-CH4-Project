#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGamePlayGameMode.generated.h"

UCLASS()
class DEEPRAIDERS_API ADRGamePlayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRGamePlayGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	virtual void BeginPlay() override;
};
