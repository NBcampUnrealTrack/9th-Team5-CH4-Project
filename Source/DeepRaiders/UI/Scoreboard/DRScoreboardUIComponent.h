#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRScoreboardUIComponent.generated.h"

class UDRScoreboardWidget;
class UDRUIManagerSubsystem;


/**
 * PlayerController가 소유하는 Scoreboard UI 생명주기 Component.
 *
 * ViewModel 내용은 모른다.
 *
 * 화면 생성 / 표시 / 숨김만 담당한다.
 */
UCLASS()
class DEEPRAIDERS_API UDRScoreboardUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRScoreboardUIComponent();

	void ShowScoreboard();

	void HideScoreboard();

	bool IsScoreboardVisible() const;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	UPROPERTY(Transient)
	TObjectPtr<UDRScoreboardWidget> ScoreboardWidget;
};
