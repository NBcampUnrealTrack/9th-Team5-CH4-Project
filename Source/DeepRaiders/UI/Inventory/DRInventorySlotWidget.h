// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Inventory/DRInventoryTypes.h"
#include "DRInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRInventorySlotClicked, FGuid, EntryId);

UCLASS()
class DEEPRAIDERS_API UDRInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetEntry(const FDRInventoryEntry& Entry);
	
	void ClearSlot();
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventorySlotClicked OnSlotClickedDelegate;
	
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
	
private:
	FGuid EntryId;	
};
