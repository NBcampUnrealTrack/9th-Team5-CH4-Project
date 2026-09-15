#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRQuickSlotViewModel.generated.h"

class UDRItemDefinition;
class UDRQuickSlotComponent;
class UTexture2D;

/** 슬롯 패널의 퀵슬롯 한 칸을 표현한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRQuickSlotEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void SelectSlot();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	FText SlotNumberText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	TObjectPtr<UTexture2D> ItemIcon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	FText QuantityText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	float ActivationIntervalProgress = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	bool bIsActivationIntervalActive = false;
	
private:
	friend class UDRQuickSlotViewModel;

	void Initialize(UDRQuickSlotComponent* InQuickSlotComponent, int32 InSlotIndex);
	void Refresh();

	bool RefreshActivationInterval();
	
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
};

/** QuickSlotComponent를 패널 항목 목록으로 변환한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRQuickSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void Initialize(UDRQuickSlotComponent* InQuickSlotComponent);

	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void Deinitialize();

protected:
	/** WBP 패널의 Viewmodel Extension -> Set Items에 바인딩할 슬롯 목록이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	TArray<TObjectPtr<UDRQuickSlotEntryViewModel>> SlotEntries;

private:
	UFUNCTION()
	void HandleQuickSlotsChanged();

	UFUNCTION()
	void HandleQuickSlotCountChanged(int32 NewSlotCount);

	UFUNCTION()
	void HandleSelectedSlotChanged(int32 PreviousSlotIndex, int32 NewSlotIndex);

	void RebuildSlotEntries();
	void RefreshSlotEntries();

	UFUNCTION()
	void HandleActivationIntervalChanged();
	
	void RefreshActivationIntervals();
	void StopActivationIntervalTimer();
	
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
	
	FTimerHandle ActivationIntervalTimerHandle;
};
