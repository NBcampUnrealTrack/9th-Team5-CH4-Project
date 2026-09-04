// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/GAS/DRAbilitySet.h"
#include "DRQuickSlotComponent.generated.h"

class UDRInventoryComponent;
class UDRItemDefinition;
class UDRWeaponUpgradeProfile;
class UAbilitySystemComponent;
struct FAbilityEndedData;

// 모든 퀵슬롯 변경 사항
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRQuickSlotsChanged);	
// 퀵슬롯의 수가 변경됨
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRQuickSlotCountChanged, int32, NewSlotCount); 
// 선택한 슬롯 인덱스 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRSelectedQuickSlotIndexChanged, int32, PreviousSlotIndex, int32, NewSlotIndex); 
// 슬롯 내의 아이템 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRSelectedQuickSlotItemChanged, UDRItemDefinition*, ItemDefinition);
// 슬롯 전환 인터벌
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRQuickSlotActivationIntervalChanged);

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
	
	// Direction이 양수면 다음 슬롯, 음수면 이전 슬롯을 선택
	// 빈 슬롯은 건너뛰고 마지막 슬롯과 첫 슬롯을 순환한다.
	void RequestSelectAdjacentSlot(int32 Direction);
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSlotCount() const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	int32 GetSelectedSlotIndex() const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot")
	FGuid GetSelectedInstanceId() const
	{
		return SelectedInstanceId;
	}
	
	/*
 	* 원격 클라이언트의 로컬 발사 간격을 검사한다.
 	* 서버 쿨다운 판정에는 사용하지 않는다.
 	*/
	bool CanRequestLocalWeaponShot(const FGuid& WeaponInstanceId) const;

	void RecordLocalWeaponShot(const FGuid& WeaponInstanceId, float FireInterval);
	
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
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot|Activation Interval")
	bool IsQuickSlotActivationIntervalActive() const;
	
	UFUNCTION(BlueprintPure, Category = "Quick Slot|Activation Interval")
	bool GetQuickSlotActivationIntervalState(int32 SlotIndex, float& OutProgress) const;
	
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
	
	int32 FindAdjacentAvailableSlotIndex(int32 StartSlotIndex, int32 Direction);
	
	// 인벤토리 내 변경에도 현재 선택중인 ItemInstance가 유효한지 검사
	// ItemInstance가 유효하지 않아 변경이 필요한 경우, 기본 무기를 우선 선택한다.
	bool EnsureValidSelection();
	
	const FDRItemInstance* ResolveSelectedItem() const;
	
	void RefreshDerivedState();	
	// 손에 든 장비에 따라 ASC의 Ability, Effect 또한 함께 새로고침
	void RefreshHeldItem();
	void RefreshEquippedWeaponUpgrade(const FDRItemInstance* SelectedItem);
	void RemoveEquippedWeaponUpgradeEffect();
	void RequestReplicationUpdate() const;
	
	// 특정 액션을 직접 알지 않고, 공통 이동 액션 활성 태그만 검사한다.
	bool IsQuickSlotSelectionLocked() const;
	
	bool CacheAbilitySystemComponent();
	void UnbindAbilitySystemComponent();
	
	void RefreshQuickSlotCollectionState();
	void RefreshSelectedItemState();
	
	// 현재 장착 아이템에서 부여된 Ability가 하나라도 활성 상태인지 검사
	bool HasActiveHeldItemAbility() const;
	
	bool ShouldDeferHeldItemRefresh() const;
	
	void QueueDeferredHeldItemRefresh();
	void ApplyDeferredHeldItemRefresh();
	void ClearDeferredHeldItemRefresh();
	
	void HandleAbilityEnded(const FAbilityEndedData& AbilityEndedData);
	
	void StartLocalQuickSlotActivationInterval(const FDRItemInstance& ItemInstance);
	void ApplyAuthorityQuickSlotActivationInterval(const FDRItemInstance& ItemInstance);
	void ClearAuthorityQuickSlotActivationInterval();
	
	bool QueryAuthorityQuickSlotActivationInterval(float& OutRemaining, float& OutDuration) const;
	double GetQuickSlotActivationIntervalTime() const;
	
	void HandleQuickSlotActivationIntervalTagChanged(FGameplayTag Tag, int32 NewCount);
	
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

	// 퀵슬롯 아이템 인터벌 변경 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Quick Slot")
	FDRQuickSlotActivationIntervalChanged OnQuickSlotActivationIntervalChangedDelegate;
	
private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	FDelegateHandle AbilityEndedDelegateHandle;
	FTimerHandle DeferredHeldItemRefreshTimerHandle;	
	
	uint8 bHeldItemRefreshDeferred = false;
	
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
	FActiveGameplayEffectHandle EquippedWeaponUpgradeEffectHandle;
	TWeakObjectPtr<UDRWeaponUpgradeProfile> EquippedWeaponUpgradeProfile;
	
	/*
 	* Ability Spec보다 오래 유지되어 무기 교체 후에도 이전 무기의
 	* 로컬 발사 가능 시각이 초기화되지 않는다.
 	*
 	* 서버 권위 상태가 아니며 복제하지 않는다.
 	*/
	TMap<FGuid, double> LocalNextWeaponFireTime;
	
	FDelegateHandle QuickSlotActivationIntervalTagChangedDelegateHandle;
	FActiveGameplayEffectHandle AuthorityQuickSlotActivationIntervalEffectHandle;
	
	// 서버 검증에 통과한 Interval과 로컬의 현재 Interval이 동일한지 검사하기 위한 Id
	// 서버의 Interval 통과 호출이 잘못된 아이템에 전달될 수 있으므로
	FGuid LocalActivationIntervalInstanceId;
	double LocalActivationIntervalStartTime = 0.0;
	double LocalActivationIntervalEndTime = 0.0;
	float LocalActivationIntervalDuration = 0.0f;
};
