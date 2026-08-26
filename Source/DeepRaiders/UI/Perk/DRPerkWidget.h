#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPerkWidget.generated.h"

class UDRPerkComponent;
class UDRPerkEntryViewModel;
class UDRPerkSlotWidget;
class UDRPerkViewModel;
class UUniformGridPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRPerkClickedSignature, FGuid, PerkInstanceId);

/** 퍽 ViewModel의 슬롯 목록을 표시한다. */
UCLASS()
class DEEPRAIDERS_API UDRPerkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializePerks(UDRPerkComponent* NewPerkComponent);

	UFUNCTION(BlueprintCallable, Category = "Perk|MVVM")
	void SetPerkEntries(const TArray<UDRPerkEntryViewModel*>& NewPerkEntries);

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerkClickedSignature OnPerkClicked;

protected:
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleSlotClicked(FGuid PerkInstanceId);

	UPROPERTY(EditDefaultsOnly, Category = "Perk|MVVM")
	FName PerkViewModelName = TEXT("DRPerkViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Perk|MVVM")
	TSubclassOf<UDRPerkSlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Perk|UI", meta = (ClampMin = "1"))
	int32 PerkSlotsPerRow = 5;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> SlotPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRPerkViewModel> PerkViewModel;
};
