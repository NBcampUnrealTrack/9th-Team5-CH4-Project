// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRInventoryWidget.generated.h"

class  UButton;
class UDRInventoryComponent;
class UDRInventorySlotWidget;
class UUniformGridPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRInventoryEntryClicked, FGuid, EntryId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRInventoryCloseRequested);

UCLASS()
class DEEPRAIDERS_API UDRInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 이 위젯과 연결될 InventoryComponent 설정
	void InitializeInventory(UDRInventoryComponent* NewInventoryComponent);
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryEntryClicked OnEntryClickedDelegate;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryCloseRequested OnCloseRequestedDelegate;
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
private:
	void BindInventory();
	void UnBindInventory();
	void RebuildSlot();
	void RefreshSlots();
	
	UFUNCTION()
	void HandleInventoryChanged();
	
	UFUNCTION()
	void HandleSlotClicked(FGuid EntryId);
	
	UFUNCTION()
	void HandleCloseClicked();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UDRInventorySlotWidget> InventorySlotWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI", meta = (ClampMin = "1"))
	int32 SlotsPerRow = 5;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> SlotPanel;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;
	
private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRInventorySlotWidget>> SlotWidgets;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;	
};
