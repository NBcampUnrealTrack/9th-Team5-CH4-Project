// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Inventory/DRInventoryTypes.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DRInventoryComponent.generated.h"

class UDRItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDRInventoryItemDefinitionReplaced,
	UDRItemDefinition*,
	SourceDefinition,
	UDRItemDefinition*,
	TargetDefinition);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEEPRAIDERS_API UDRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDRInventoryComponent();
	
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// BeginPlay 전에만 호출
	// 일반 추가와 슬롯 이동으로 건드릴 수 없는 앞쪽 슬롯 수를 설정
	void SetLockedSlotCount(int32 NewLockedSlotCount);
	
	// Definition으로 새 인스턴스를 생성하여 추가
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	bool TryAddItem(UDRItemDefinition* Definition, int32 Quantity);
	
	// 월드 아이템의 InstanceId와 RuntimeState를 유지하며 추가
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	bool TryAddItemInstance(const FDRItemInstance& ItemInstance);
	
	// 아이템을 지정 위치에 새 인스턴스를 생성, ex) 시작 아이템
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	bool TryAddItemToSlot(int32 SlotIndex, UDRItemDefinition* Definition, int32 Quantity);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryReplaceItemDefinition(
		FGuid EntryId,
		UDRItemDefinition* ExpectedSourceDefinition,
		UDRItemDefinition* TargetDefinition);
	
	// 특정 인스턴스에서 요청한 수량을 제거
	// 수량 부족 시 실패
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveFromItemInstance(FGuid InstanceId, int32 Quantity);
	
	// 동일한 Definition을 가진 여러 엔트리에서 요청 수량을 제거
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveItemByDefinition(UDRItemDefinition* Definition, int32 Quantity);

	/** 지정된 인스턴스를 모두 검증하고 변경 알림 한 번으로 일괄 제거한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveItemInstances(const TArray<FGuid>& InstanceIds);
	
	// SourceInstanceId가 가리키는 Instance를 DestinationInventory로 이동시킨다.
	// 서버에서만 실행, 실제로 이동한 수량 반환, 요청한 수량의 처리가 불가능한 경우 실패
	// 현재 1개의 슬롯 이동만 지원
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 TryTransferFromItemInstance(UDRInventoryComponent* DestinationInventory, FGuid SourceInstanceId, int32 RequestedQuantity);
	
	// 로컬 플레이어에서 슬롯 위치 교환 요청
	UFUNCTION(BlueprintCallable)
	void RequestSwapSlots(int32 SourceSlotIndex, int32 TargetSlotIndex);
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(UDRItemDefinition* Definition, int32 Quantity) const;
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItemInstance(const FDRItemInstance& ItemInstance) const;
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetAddableQuantity(UDRItemDefinition* Definition) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UDRItemDefinition* Definition) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const
	{
		return MaxSlots;
	}

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetLockedSlotCount() const
	{
		return LockedSlotCount;
	}
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSlotLocked(int32 SlotIndex) const
	{
		return SlotIndex >= 0 && SlotIndex < LockedSlotCount;
	}

	const TArray<FDRItemInstance>& GetItemInstances() const
	{
		return Slots;
	}
	
	const FDRItemInstance* GetItemInstance(FGuid InstanceId) const;
	
	// 슬롯 위치 조회
	const FDRItemInstance* GetItemAtSlot(int32 SlotIndex) const;
	
	// InstanceId로 인스턴스를 조회
	const FDRItemInstance* FindItemInstance(FGuid InstanceId) const;
	
	// InstanceId가 있는 슬롯을 반환
	int32 FindSlotIndex(FGuid InstanceId) const;
	
	// 서버에서 ItemInstance의 RuntimeState를 안전하게 수정하는 함수
	bool ModifyItemInstance(FGuid InstanceId, TFunctionRef<bool(FDRItemInstance&)> Modifier);
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestSwapSlots(int32 SourceSlotIndex, int32 TargetSlotIndex
		, FGuid ExpectedSourceInstanceId, FGuid ExpectedTargetInstanceId);
	
	UFUNCTION()
	void OnRep_Slots();

private:
	bool HasInventoryAuthority() const;
	bool IsValidSlotIndex(int32 SlotIndex) const;
	
	int32 GetMaxStackSize(const UDRItemDefinition* Definition) const;
	
	int32 FindFirstEmptyUnlockedSlot() const;
	
	FGuid GetInstanceIdAtSlot(int32 SlotIndex) const;
	
	// 서버에서 호출될 인벤토리 변경 처리 함수
	void HandleInventoryChangedOnServer();
	
	void BroadcastInventoryChanged();

	// 인벤토리 간 통신을 위한 변경 알림 없는 추가/제거 함수
	// 기존 함수 활용 시, 제거 -> 알림 -> 추가 -> 알림 순서가 강제됨
	// 변경 알림 없이 아이템을 추가
	void AddItemInternal(UDRItemDefinition* Definition, int32 Quantity);
	
	void AddItemInstanceInternal(const FDRItemInstance& ItemInstance);
	
	// 변경 알림 없이 지정된 Entry의 아이템을 제거
	void RemoveFromSlotInternal(int32 SlotIndex, int32 Quantity);
	
	bool SwapSlotsInternal(int32 SourceSlotIndex, int32 TargetSlotIndex);
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryChanged OnInventoryChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryItemDefinitionReplaced OnItemDefinitionReplacedDelegate;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSlots = 5;
	
	// 앞에서부터 일반 추가와 위치 교환이 금지되는 슬롯의 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0", UIMin = "0"))
	int32 LockedSlotCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slots, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FDRItemInstance> Slots;
};
