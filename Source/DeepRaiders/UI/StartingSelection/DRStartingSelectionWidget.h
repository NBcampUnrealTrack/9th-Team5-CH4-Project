#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRStartingSelectionWidget.generated.h"

class UDRStartingSkillSelectWidget;
class UDRStartingWeaponSelectWidget;
class UDRStartingSelectionComponent;

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
	void HandleSelectionAvailabilityChanged(bool IsAvailable);
	void DeinitializeSelection();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRStartingWeaponSelectWidget> WeaponPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRStartingSkillSelectWidget> SkillPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingSelectionComponent> SelectionComponent;
};
