#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRStartingSelectionUIComponent.generated.h"

class ADRPlayerController;
class ADRMiningGameStateBase;
class UDRStartingSelectionWidget;
class UDRStartingSelectionComponent;
class UDRUIManagerSubsystem;

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRStartingSelectionUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRStartingSelectionUIComponent();

	void InitializeStartingSelection(
		UDRStartingSelectionComponent* InSelectionComponent);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ShowStartingSelection();

	UFUNCTION()
	void HandleGameTimerChanged(
		int32 RemainingSeconds,
		bool IsGameStarted,
		bool IsGameEnded);

	UFUNCTION()
	void HideStartingSelection();

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingSelectionWidget> StartingSelectionWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingSelectionComponent> SelectionComponent;

	UPROPERTY(Transient)
	TObjectPtr<ADRMiningGameStateBase> MiningGameState;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	bool IsMoveInputBlocked = false;
};
