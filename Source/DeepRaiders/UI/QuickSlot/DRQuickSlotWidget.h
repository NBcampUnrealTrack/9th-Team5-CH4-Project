// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRQuickSlotWidget.generated.h"

class UDRQuickSlotComponent;
class UDRQuickSlotViewModel;
class UHorizontalBox;

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
private:
	/** Widget Blueprint에 등록한 Manual ViewModel 이름과 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|MVVM")
	FName QuickSlotViewModelName = TEXT("DRQuickSlotViewModel");

	/** MVVM 패널 확장이 런타임에 슬롯 패널을 읽을 수 있도록 공개한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot|MVVM",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHorizontalBox> SlotPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotViewModel> QuickSlotViewModel;
};
