// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Inventory/DRInventoryTypes.h"
#include "DRInventoryComponent.generated.h"

class UDRItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDRInventoryEntryDefinitionReplaced,
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
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 요청 수량 전체를 인벤토리에 추가
	// 일부 추가 미구현
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryAddItem(UDRItemDefinition* Definition, int32 Quantity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryReplaceEntryDefinition(
		FGuid EntryId,
		UDRItemDefinition* ExpectedSourceDefinition,
		UDRItemDefinition* TargetDefinition);
	
	// 특정 엔트리에서 요청한 수량을 제거
	// 수량 부족 시 실패
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveFromEntry(FGuid EntryId, int32 Quantity);
	
	// 동일한 Definition을 가진 여러 엔트리에서 요청 수량을 제거
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveItemByDefinition(UDRItemDefinition* Definition, int32 Quantity);

	/** 지정된 엔트리를 모두 검증하고 변경 알림 한 번으로 일괄 제거한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveEntries(const TArray<FGuid>& EntryIds);
	
	// SourceEntryId가 가리키는 Entry에서 DestinationInventory로 이동시킨다.
	// 서버에서만 실행, 실제로 이동한 수량 반환, 요청한 수량의 처리가 불가능한 경우 실패
	// 현재 1개의 슬롯 이동만 지원
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 TryTransferFromEntry(UDRInventoryComponent* DestinationInventory, FGuid SourceEntryId, int32 RequestedQuantity);
	
	// EntryId에 해당하는 엔트리를 탐색
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindEntry(FGuid EntryId, FDRInventoryEntry& OutEntry) const;
	
	// 요청한 수량 전체를 추가할 수 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(UDRItemDefinition* Definition, int32 Quantity) const;
	
	// 현재 인벤토리에 추가할 수 있는 최대 수량 반환
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetAddableQuantity(UDRItemDefinition* Definition) const;
	
	// 특정 Definition의 보유 수량을 반환
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UDRItemDefinition* Definition) const;
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FDRInventoryEntry> GetEntries() const
	{
		return Entries;
	}
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetOccupiedSlotCount() const
	{
		return Entries.Num();
	}
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const
	{
		return MaxSlots;
	}
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsEmpty() const
	{
		return Entries.IsEmpty();
	}
	
protected:
	// 클라이언트가 새로운 인벤토리 배열을 받았을 때 호출
	UFUNCTION()
	void OnRep_Entries();

private:
	bool HasInventoryAuthority() const;
	int32 GetMaxStackSize(const UDRItemDefinition* Definition) const;
	
	// 서버에서 호출될 인벤토리 변경 처리 함수
	void HandleInventoryChangedOnServer();
	
	void BroadcastInventoryChanged();

	// 인벤토리 간 통신을 위한 변경 알림 없는 추가/제거 함수
	// 기존 함수 활용 시, 제거 -> 알림 -> 추가 -> 알림 순서가 강제됨
	// 변경 알림 없이 아이템을 추가
	void AddItemInternal(UDRItemDefinition* Definition, int32 Quantity);
	// 변경 알림 없이 지정된 Entry의 아이템을 제거
	void RemoveFromEntryInternal(int32 EntryIndex, int32 Quantity);
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryChanged OnInventoryChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryEntryDefinitionReplaced OnEntryDefinitionReplacedDelegate;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSlots = 5;

	UPROPERTY(ReplicatedUsing = OnRep_Entries, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FDRInventoryEntry> Entries;
};
