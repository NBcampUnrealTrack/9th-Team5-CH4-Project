// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/GAS/DRAbilitySet.h"
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

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEEPRAIDERS_API UDRQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UDRQuickSlotComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// 숫자 키에 해당하는 슬롯을 선택하도록 요청
	// 비어있는 경우도 선택 가능하며 이 경우, 빈 손이 된다.
	UFUNCTION(BlueprintCallable, Category = "Quick Slot")
	void RequestSelectSlot(int32 SlotIndex);
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSlotCount() const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSelectedSlotIndex() const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	FGuid GetSelectedInstanceId() const
	{
		return SelectedInstanceId;
	}
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	bool GetQuickSlot(int32 SlotIndex, FDRItemInstance& OutItemInstance) const;
	
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
	
	void RefreshSelectedItem();
	
	// Character에게 SelectedItem 외형 반영
	void ApplySelectedItemToCharacter();
	
protected:

	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(int32 SlotIndex, FGuid ExpectedInstanceId);
	
	UFUNCTION()
	void OnRep_SelectedInstanceId();
	
	UFUNCTION()
	void HandleInventoryChanged();
	
private:
	bool CachedInventoryComponent();
	
	bool HasQuickSlotAuthority() const;
	bool IsLocalPlayer() const;
	
	bool SelectSlotInternal(int32 SlotIndex, FGuid ExpectedInstanceId);
	
	// 인벤토리 내 변경에도 현재 선택중인 ItemInstance가 유효한지 검사
	// ItemInstance가 유효하지 않아 변경이 필요한 경우, 기본 무기를 우선 선택한다.
	bool EnsureValidSelection();
	
	const FDRItemInstance* ResolveSelectedItem() const;
	
	void RefreshDerivedState();	
	// 손에 든 장비에 따라 ASC의 Ability, Effect 또한 함께 새로고침
	void RefreshHeldItem();
	void RequestReplicationUpdate() const;
	
public:
	// 모든 퀵슬롯 변경에 호출
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

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	
	UPROPERTY(ReplicatedUsing = OnRep_SelectedInstanceId, VisibleInstanceOnly, BlueprintReadOnly
		, Category = "Quick Slot", meta = (AllowPrivateAccess = "true"))
	FGuid SelectedInstanceId;
	
	/**
 	* UI에 슬롯 수 변경을 알리기 위해
 	* 마지막으로 확인한 슬롯 수를 보관한다.
 	*/
	int32 CachedSlotCount = 0;
	int32 CachedSelectedSlotIndex = INDEX_NONE;
	
	// 실제로 손에 쥐어질 아이템의 Definition
	// 수량이 0인 경우 nullptr
	UPROPERTY(Transient)
	TObjectPtr<UDRItemDefinition> HeldItemDefinition;
	
	FGuid EquippedInstanceId;
	FDRAbilitySet_GrantedHandles GrantedHandles;
};
