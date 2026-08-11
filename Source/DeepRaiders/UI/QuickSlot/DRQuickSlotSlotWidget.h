// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRQuickSlotSlotWidget.generated.h"

class UButton;
class UBorder;
class UDRItemDefinition;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRQuickSlotClicked, int32, SlotIndex);

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetSlotData(int32 NewSlotIndex, UDRItemDefinition* Definition, int32 Quantity, bool bIsSelected, bool bIsAvailable);

	UPROPERTY(BlueprintAssignable, Category = "Quick Slot")
	FDRQuickSlotClicked OnQuickSlotClickedDelegate;
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleSlotClicked();
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotNumberText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> SelectionBorder;
	
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|UI")
	FLinearColor SelectedColor = FLinearColor::White;
	
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|UI")
	FLinearColor UnselectedColor = FLinearColor::Transparent;
	
private:
	int32 SlotIndex = INDEX_NONE;
	
};
