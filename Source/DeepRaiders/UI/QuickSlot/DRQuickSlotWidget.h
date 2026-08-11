// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRQuickSlotWidget.generated.h"

class UDRQuickSlotComponent;
class UDRQuickSlotSlotWidget;
class UUniformGridPanel;

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 이 위젯과 연결될 QuickSlotComponent 설정
	void InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent);
	
protected:
	virtual void NativeDestruct() override;
	
private:
	void BindQuickSlot();
	void UnbindQuickSlot();
	void RebuildSlots();
	void RefreshSlots();
	
	UFUNCTION()
	void HandleQuickSlotsChanged();
	
	// 슬롯의 수가 변경됨
	UFUNCTION()
	void HandleQuickSlotCountChanged(int32 NewSlotCount);
	
	UFUNCTION()
	void HandleSelectedSlotChanged(int32 PreviousSlotIndex, int32 NewSlotIndex);
	
	UFUNCTION()
	void HandleSlotClicked(int32 SlotIndex);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|UI")
	TSubclassOf<UDRQuickSlotSlotWidget> QuickSlotSlotWidgetClass;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> SlotPanel;
	
private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRQuickSlotSlotWidget>> SlotWidgets;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
};
