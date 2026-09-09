// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRQuickSlotWidget.generated.h"

class UDRQuickSlotComponent;
class UDRQuickSlotEntryViewModel;
class UDRQuickSlotViewModel;

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent);

	/** WBP에 미리 배치한 슬롯에 각 엔트리 ViewModel을 연결한다. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Quick Slot|MVVM")
	void SetSlotEntries(const TArray<UDRQuickSlotEntryViewModel*>& NewSlotEntries);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
private:
	/** Widget Blueprint에 등록한 Manual ViewModel 이름과 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|MVVM")
	FName QuickSlotViewModelName = TEXT("DRQuickSlotViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotViewModel> QuickSlotViewModel;
};
