#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPerkWidget.generated.h"

class UDRPerkComponent;
class UDRPerkEntryViewModel;
class UDRPerkViewModel;
class UUniformGridPanel;

/** 퍽 ViewModel의 슬롯 목록을 표시한다. */
UCLASS()
class DEEPRAIDERS_API UDRPerkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializePerks(UDRPerkComponent* NewPerkComponent);

	UFUNCTION(BlueprintCallable, Category = "Perk|MVVM")
	void SetPerkEntries(const TArray<UDRPerkEntryViewModel*>& NewPerkEntries);

protected:
	virtual void NativeDestruct() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Perk|MVVM")
	FName PerkViewModelName = TEXT("DRPerkViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Perk|MVVM")
	FName EntryViewModelName = TEXT("DRPerkEntryViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Perk|MVVM")
	TSubclassOf<UUserWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Perk|UI", meta = (ClampMin = "1"))
	int32 PerkSlotsPerRow = 5;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> SlotPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRPerkViewModel> PerkViewModel;
};
