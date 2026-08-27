#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRStartingSelectionWidget.generated.h"

class UDRStartingSkillSelectWidget;
class UDRStartingWeaponSelectWidget;
class UDRStartingSelectionComponent;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRStartingSelectionCompletedSignature);

UCLASS()
class DEEPRAIDERS_API UDRStartingSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSelection(
		UDRStartingSelectionComponent* InSelectionComponent);

	UPROPERTY(BlueprintAssignable, Category = "Starting Selection|UI")
	FDRStartingSelectionCompletedSignature OnSelectionCompleted;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void HandleSelectionAvailabilityChanged(bool);
	void DeinitializeSelection();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRStartingWeaponSelectWidget> WeaponPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRStartingSkillSelectWidget> SkillPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> SelectionPanelSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingSelectionComponent> SelectionComponent;
};
