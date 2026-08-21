#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "DRMiningGameModeBase.generated.h"

// 채굴 테스트/플레이용 GameState를 사용하는 GameMode이다.
UCLASS()
class DEEPRAIDERS_API ADRMiningGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRMiningGameModeBase();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	void StartTimer();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	void EndTimer();

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "0.01", Units = "s"))
	float PassiveCoinInterval = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "1"))
	int32 PassiveCoinAmount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "0.0", Units = "s"))
	float PassiveCoinIncreaseInterval = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "0"))
	int32 PassiveCoinIncreaseAmount = 0;

private:
	void GrantPassiveCoins();
	int64 GetPassiveCoinAmountAtGrantIndex(int64 GrantIndex) const;

	FTimerHandle PassiveCoinTimerHandle;
	double PassiveCoinStartTime = 0.0;
	int64 LastProcessedGrantIndex = 0;
};
