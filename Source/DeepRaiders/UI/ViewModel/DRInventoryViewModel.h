#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRInventoryViewModel.generated.h"

class UDRInventoryComponent;
class UDRQuickSlotComponent;
class UDRItemDefinition;
class UTexture2D;
class ADRPlayerState;
class APlayerController;
struct FDRPublicQuickSlot;

/** 인벤토리 슬롯 한 칸의 표시 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRInventorySlotEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	int32 GetSlotIndex() const
	{
		return SlotIndex;
	}

	FGuid GetInstanceId() const
	{
		return InstanceId;
	}

	bool IsLocked() const
	{
		return bIsLocked;
	}

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	FText SlotNumberText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	TObjectPtr<UTexture2D> ItemIcon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	FText QuantityText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	bool bIsLocked = false;

private:
	friend class UDRInventoryViewModel;

	void Initialize(UDRInventoryComponent* InInventoryComponent, int32 InSlotIndex);
	void Initialize(UDRQuickSlotComponent* InQuickSlotComponent, int32 InSlotIndex);
	void Initialize(const FDRPublicQuickSlot& InSlot, int32 InSlotIndex);
	void Refresh();

	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
};

/** InventoryComponent를 슬롯 ViewModel 목록으로 변환한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UDRInventoryComponent* GetInventoryComponent() const
	{
		return InventoryComponent.Get();
	}

	TArray<UDRInventorySlotEntryViewModel*> GetQuickSlotEntries() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Initialize(UDRInventoryComponent* InInventoryComponent);

	void Initialize(UDRQuickSlotComponent* InQuickSlotComponent);

	/** 팀원 PlayerState의 공개 스냅샷을 표시한다. */
	void Initialize(ADRPlayerState* InPlayerState, bool bInIsLocalPlayer);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	bool bIsLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	bool bIsOccupied = false;

	/** 장착 상태를 제외한 퀵슬롯 표시 목록이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quick Slot")
	TArray<TObjectPtr<UDRInventorySlotEntryViewModel>> QuickSlotEntries;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	void RebuildQuickSlotEntries();

	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
	TWeakObjectPtr<ADRPlayerState> PlayerState;
};

/** 로컬 플레이어를 중앙에 고정한 3인 인벤토리 화면 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRInventoryScreenViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(APlayerController* InPlayerController);

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	TObjectPtr<UDRInventoryViewModel> LeftPanel;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	TObjectPtr<UDRInventoryViewModel> CenterPanel;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory")
	TObjectPtr<UDRInventoryViewModel> RightPanel;
};
