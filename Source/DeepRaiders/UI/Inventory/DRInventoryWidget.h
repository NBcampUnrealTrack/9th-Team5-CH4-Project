// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRInventoryWidget.generated.h"

class  UButton;
class UDRInventoryComponent;
class UDRInventorySlotEntryViewModel;
class UDRInventorySlotWidget;
class UDRInventoryViewModel;
class UHorizontalBox;
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

	/** Screen ViewModel이 생성한 플레이어 패널 ViewModel을 주입한다. */
	void InitializeViewModel(UDRInventoryViewModel* NewViewModel);

	/** ViewModel 슬롯 목록을 UniformGrid 엔트리 위젯으로 변환한다. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|MVVM")
	void SetSlotEntries(const TArray<UDRInventorySlotEntryViewModel*>& NewSlotEntries);

	/** ViewModel 퀵슬롯 목록을 가로 패널에 표시한다. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|MVVM")
	void SetQuickSlotEntries(const TArray<UDRInventorySlotEntryViewModel*>& NewQuickSlotEntries);
	
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
	void HandleSlotClicked(FGuid InstanceId);
	
	UFUNCTION()
	void HandleMoveRequested(int32 SourceSlotIndex, int32 TargetSlotIndex);
	
	UFUNCTION()
	void HandleCloseClicked();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UDRInventorySlotWidget> InventorySlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UDRInventorySlotWidget> QuickSlotWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI", meta = (ClampMin = "1"))
	int32 SlotsPerRow = 5;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> SlotPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> QuickSlotPanel;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;
	
private:
	/** Widget Blueprint에 등록한 Manual ViewModel 이름이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|MVVM")
	FName InventoryViewModelName = TEXT("DRInventoryViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRInventorySlotWidget>> SlotWidgets;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;	
};
