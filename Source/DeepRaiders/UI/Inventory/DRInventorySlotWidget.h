// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DRInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRInventorySlotClicked, FGuid, InstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRInventorySlotMoveRequested, int32, SourceSlotIndex, int32, TargetSlotIndex);

UCLASS()
class DEEPRAIDERS_API UDRInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetItemInstance(int32 InSlotIndex, const FDRItemInstance& ItemInstance, bool bInLocked);
	
	void ClearSlot(int32 InSlotIndex, bool bInLocked);
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventorySlotClicked OnSlotClickedDelegate;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventorySlotMoveRequested OnMoveRequestedDelegate;
	
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
		
protected:
	virtual void NativeConstruct() override;

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
	int32 SlotIndex = INDEX_NONE;
	FGuid InstanceId;	
	uint8 bLocked:1 = false;
	
	uint8 bPointerPressed : 1 = false;
};
