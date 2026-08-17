// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRQuickSlotWidget.generated.h"

class UDRQuickSlotComponent;
class UDRQuickSlotEntryViewModel;
class UDRQuickSlotSlotWidget;
class UDRQuickSlotViewModel;
class UHorizontalBox;

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent);

	/** ViewModel의 슬롯 목록을 실제 엔트리 위젯으로 표시한다. */
	UFUNCTION(BlueprintCallable, Category = "Quick Slot|MVVM")
	void SetSlotEntries(const TArray<UDRQuickSlotEntryViewModel*>& NewSlotEntries);
	
protected:
	virtual void NativeDestruct() override;
	
private:
	/** Widget Blueprint에 등록한 Manual ViewModel 이름과 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|MVVM")
	FName QuickSlotViewModelName = TEXT("DRQuickSlotViewModel");

	/** 슬롯 엔트리 Widget Blueprint에 등록한 Manual ViewModel 이름이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|MVVM")
	FName EntryViewModelName = TEXT("QuickSlotEntryViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|MVVM")
	TSubclassOf<UDRQuickSlotSlotWidget> SlotWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SlotPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotViewModel> QuickSlotViewModel;
};
