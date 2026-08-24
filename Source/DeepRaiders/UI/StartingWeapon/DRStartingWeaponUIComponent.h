#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRStartingWeaponUIComponent.generated.h"

class ADRPlayerController;
class UDRStartingWeaponSelectWidget;
class UDRUIManagerSubsystem;

UCLASS()
class DEEPRAIDERS_API UDRStartingWeaponUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRStartingWeaponUIComponent();

	void OpenSelection();
	void CloseSelection();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryInitialize();

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingWeaponSelectWidget> SelectionWidget;

	bool IsSelectionOpened = false;
};
