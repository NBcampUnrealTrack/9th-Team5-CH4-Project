// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRQuickSlotComponent.generated.h"

class UDRInventoryComponent;
class UDRItemDefinition;

// 모든 퀵슬롯 변경 사항
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRQuickSlotsChanged);	
// 퀵슬롯의 수가 변경됨
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRQuickSlotCountChanged, int32, NewSlotCount); 
// 선택한 슬롯 인덱스 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRSelectedQuickSlotIndexChanged, int32, PreviousSlotIndex, int32, NewSlotIndex); 
// 슬롯 내의 아이템 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRSelectedQuickSlotItemChanged, UDRItemDefinition*, ItemDefinition);

USTRUCT(BlueprintType)
struct FDRQuickSlotEntry
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Quick Slot")
	TObjectPtr<UDRItemDefinition> Definition = nullptr;	
	
	bool IsBound() const
	{
		return Definition != nullptr;
	}
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEEPRAIDERS_API UDRQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UDRQuickSlotComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
	// 서버에서 빈 퀵슬롯에 아이템 등록 시도
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quick Slot")
	bool TryBindFirstEmptySlot(UDRItemDefinition* Definition);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quick Slot")
	bool TryBindSelectedSlot(UDRItemDefinition* Definition);
	
	// 로컬 플레이어가 특정 슬롯에 아이템 바인딩 요청
	// 서버는 플레이어 인벤토리에 해당 아이템이 있는지 검증
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void RequestBindSlot(int32 SlotIndex, UDRItemDefinition* Definition);
	
	// 특정 퀵슬롯 바인딩 제거
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void RequestClearSlot(int32 SlotIndex);
	
	// 숫자 키에 해당하는 슬롯을 선택하도록 요청
	// 비어있는 경우도 선택 가능하며 이 경우, 빈 손이 된다.
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void RequestSelectSlot(int32 SlotIndex);
	
	// 서버에서 퀵슬롯의 전체 개수를 변경
	// 슬롯 수 감소 시 범위 밖 바인딩 제거
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quick Slot")
	bool SetSlotCount(int32 NewSlotCount);
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSlotCount() const
	{
		return QuickSlots.Num();
	}
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSelectedSlotIndex() const
	{
		return SelectedSlotIndex;
	}
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	TArray<FDRQuickSlotEntry> GetQuickSlots() const
	{
		return QuickSlots;
	}
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	bool GetQuickSlot(int32 SlotIndex, FDRQuickSlotEntry& OutSlot) const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	bool IsSlotBound(int32 SlotIndex) const;

	// 슬롯에 바인딩된 아이템을 인벤토리에 보유하고 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	bool IsSlotItemAvailable(int32 SlotIndex) const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSlotItemCount(int32 SlotIndex) const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	UDRItemDefinition* GetSelectedItemDefinition() const
	{
		return HeldItemDefinition;
	}	
	
	// Character에게 SelectedItem 외형 반영
	void ApplySelectedItemToCharacter();
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerBindSlot(int32 SlotIndex, UDRItemDefinition* Definition);
	
	UFUNCTION(Server, Reliable)
	void ServerClearSlot(int32 SlotIndex);
	
	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(int32 SlotIndex);
	
	UFUNCTION()
	void OnRep_QuickSlots();
	
	UFUNCTION()
	void OnRep_SelectedSlotIndex(int32 PreviousSlotIndex);
	
	UFUNCTION()
	void HandleInventoryChanged();
	
private:
	bool CacheInventoryComponent();
	
	bool HasQuickSlotAuthority() const;
	bool IsLocalPlayer() const;
	
	bool BindSlotInternal(int32 SlotIndex, UDRItemDefinition* Definition);
	
	bool ClearSlotInternal(int32 SlotIndex);
	bool SelectSlotInternal(int32 SlotIndex);
	
	UDRItemDefinition* ResolveHandedItemDefinition(int32 SlotIndex) const;
	
	void RefreshHandedItem();
	void RequestReplicationUpdate() const;
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Quick Slot")
	FDRQuickSlotsChanged OnQuickSlotsChangedDelegate;
	
	// 슬롯 개수가 변경되면 호출
	UPROPERTY(BlueprintAssignable, Category = "Quick Slot")
	FDRQuickSlotCountChanged OnQuickSlotCountChangedDelegate;
	
	// 선택한 퀵슬롯 인덱스 변경 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Quick Slot")
	FDRSelectedQuickSlotIndexChanged OnSelectedQuickSlotIndexChangedDelegate;
	
	// 퀵슬롯 설정된 아이템 Definition 변경 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Quick Slot")
	FDRSelectedQuickSlotItemChanged OnSelectedQuickSlotItemChangedDelegate;

protected:
	// 기본으로 제공되는 최초 슬롯 수
	// 실제 슬롯 수는 QuickSlots.Num()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quick Slot", meta = (ClampMin = 1, UIMin = 1))
	int32 InitialSlotCount = 5;
	
	UPROPERTY(ReplicatedUsing = OnRep_QuickSlots, VisibleInstanceOnly, BlueprintReadOnly, Category = "Quick Slot")
	TArray<FDRQuickSlotEntry> QuickSlots;
	
	UPROPERTY(ReplicatedUsing = OnRep_SelectedSlotIndex, VisibleInstanceOnly, BlueprintReadOnly, Category = "Quick Slot")
	int32 SelectedSlotIndex = INDEX_NONE;
	
private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	
	/**
 	* UI에 슬롯 수 변경을 알리기 위해
 	* 마지막으로 확인한 슬롯 수를 보관한다.
 	*/
	int32 CachedSlotCount = 0;
	
	// 실제로 손에 쥐어질 아이템의 Definition
	// 수량이 0인 경우 nullptr
	UPROPERTY(Transient)
	TObjectPtr<UDRItemDefinition> HeldItemDefinition;
};
