#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPerkSlotWidget.generated.h"

class UDRPerkEntryViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRPerkSlotClickedSignature, FGuid, PerkInstanceId);

UCLASS()
class DEEPRAIDERS_API UDRPerkSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeViewModel(UDRPerkEntryViewModel* NewViewModel);

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerkSlotClickedSignature OnSlotClicked;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Perk|MVVM")
	FName EntryViewModelName = TEXT("DRPerkEntryViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRPerkEntryViewModel> EntryViewModel;

	FGuid PerkInstanceId;
	bool IsPointerPressed = false;
};
