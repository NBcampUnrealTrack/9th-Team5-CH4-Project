// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventoryComponent.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/Upgrade/DRWeaponUpgradeProfile.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

UDRInventoryComponent::UDRInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UDRInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	
	MaxSlots = FMath::Max(1, MaxSlots);
	LockedSlotCount = FMath::Clamp(LockedSlotCount, 0, MaxSlots);
	
	Slots.SetNum(MaxSlots);
}

void UDRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, Slots);
}

void UDRInventoryComponent::SetLockedSlotCount(int32 NewLockedSlotCount)
{
	ensureMsgf(!HasBegunPlay(), TEXT("LockedSlotCount must be set before BeginPlay."));
	
	LockedSlotCount = FMath::Clamp(NewLockedSlotCount, 0, FMath::Max(1, MaxSlots));
}

bool UDRInventoryComponent::TryAddItem(UDRItemDefinition* Definition, int32 Quantity)
{
	if (!HasInventoryAuthority()
		|| !CanAddItem(Definition, Quantity))
	{
		return false;
	}
	
	AddItemInternal(Definition, Quantity);
	HandleInventoryChangedOnServer();
	
	return true;
}

bool UDRInventoryComponent::TryAddItemInstance(const FDRItemInstance& ItemInstance)
{
	if (!HasInventoryAuthority()
		|| !CanAddItemInstance(ItemInstance))
	{
		return false;
	}
	
	AddItemInstanceInternal(ItemInstance);
	HandleInventoryChangedOnServer();
	
	return true;
}

bool UDRInventoryComponent::TryAddItemToSlot(int32 SlotIndex, UDRItemDefinition* Definition, int32 Quantity)
{
	if (!HasInventoryAuthority()
		|| !IsValidSlotIndex(SlotIndex)
		|| Slots[SlotIndex].IsValid()
		|| !IsValid(Definition)
		|| Quantity <= 0
		|| Quantity > GetMaxStackSize(Definition))
	{
		return false;
	}
	
	FDRItemInstance NewItemInstance = DRItemInstanceFactory::Create(Definition, Quantity);
	
	if (!NewItemInstance.IsValid())
	{
		return false;
	}
	
	Slots[SlotIndex] = MoveTemp(NewItemInstance);
	HandleInventoryChangedOnServer();
	
	return true;
}

bool UDRInventoryComponent::TryReplaceItemDefinition(
	FGuid InstanceId,
	UDRItemDefinition* ExpectedSourceDefinition,
	UDRItemDefinition* TargetDefinition)
{
	if (!HasInventoryAuthority()
		|| !InstanceId.IsValid()
		|| !IsValid(ExpectedSourceDefinition)
		|| !IsValid(TargetDefinition)
		|| ExpectedSourceDefinition == TargetDefinition)
	{
		return false;
	}

	const int32 SlotIndex = FindSlotIndex(InstanceId);

	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	
	FDRItemInstance& ItemInstance = Slots[SlotIndex];
	
	if (ItemInstance.Definition != ExpectedSourceDefinition
		|| ItemInstance.Quantity != 1)
	{
		return false;
	}

	FDRItemInstance Replacement = DRItemInstanceFactory::Create(TargetDefinition, 1);
	
	if (!Replacement.IsValid())
	{
		return false;
	}
	
	Replacement.InstanceId = ItemInstance.InstanceId;
	ItemInstance = MoveTemp(Replacement);
	
	OnItemDefinitionReplacedDelegate.Broadcast(ExpectedSourceDefinition, TargetDefinition);
	
	HandleInventoryChangedOnServer();
	return true;
}

bool UDRInventoryComponent::TryRemoveItemInstance(FGuid InstanceId, int32 Quantity)
{
	if (!HasInventoryAuthority()
		|| !InstanceId.IsValid()
		|| Quantity <= 0)
	{
		return false;
	}
	
	const int32 SlotIndex = FindSlotIndex(InstanceId);
	
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].Quantity < Quantity)
	{
		return false;
	}
	
	RemoveFromSlotInternal(SlotIndex, Quantity);	
	HandleInventoryChangedOnServer();
	
	return true;
}

bool UDRInventoryComponent::TryRemoveItemByDefinition(UDRItemDefinition* Definition, int32 Quantity)
{
	if (!HasInventoryAuthority()
	|| !IsValid(Definition)
	|| Quantity <= 0
	|| GetItemCount(Definition) < Quantity)
	{
		return false;
	}
	
	int32 RemainingQuantity = Quantity;
	
	// 배열 뒤에서부터 제거
	for (int32 SlotIndex = Slots.Num() - 1; SlotIndex >= 0 && RemainingQuantity > 0 ; --SlotIndex)
	{
		FDRItemInstance& ItemInstance = Slots[SlotIndex];
		
		if (!ItemInstance.IsValid()
			|| ItemInstance.Definition != Definition)
		{
			continue;
		}
		
		const int32 RemovedQuantity = FMath::Min(ItemInstance.Quantity, RemainingQuantity);
		
		RemoveFromSlotInternal(SlotIndex, RemovedQuantity);
		
		RemainingQuantity -= RemovedQuantity;
	}
	
	HandleInventoryChangedOnServer();
	
	return true;
}

bool UDRInventoryComponent::TryRemoveItemInstanceArray(const TArray<FGuid>& InstanceIds)
{
	if (!HasInventoryAuthority()
		|| InstanceIds.IsEmpty())
	{
		return false;
	}

	TSet<FGuid> UniqueInstanceIds;

	for (const FGuid& InstanceId : InstanceIds)
	{
		if (!InstanceId.IsValid()
			|| UniqueInstanceIds.Contains(InstanceId)
			|| FindSlotIndex(InstanceId) == INDEX_NONE)
		{
			return false;
		}

		UniqueInstanceIds.Add(InstanceId);
	}

	for (FDRItemInstance& ItemInstance : Slots)
	{
		if (ItemInstance.IsValid()
			&& UniqueInstanceIds.Contains(ItemInstance.InstanceId))
		{
			ItemInstance = FDRItemInstance();
		}
	}
	
	HandleInventoryChangedOnServer();
	return true;
}

int32 UDRInventoryComponent::TryTransferFromItemInstance(UDRInventoryComponent* DestinationInventory
	, FGuid SourceInstanceId, int32 RequestedQuantity)
{
	if (!HasInventoryAuthority()
		|| !IsValid(DestinationInventory)
		|| DestinationInventory == this
		|| !DestinationInventory->HasInventoryAuthority()
		|| !SourceInstanceId.IsValid()
		|| RequestedQuantity <= 0)
	{
		return 0;
	}
	
	const int32 SourceSlotIndex = FindSlotIndex(SourceInstanceId);
	
	if (!Slots.IsValidIndex(SourceSlotIndex))
	{
		return 0;
	}
	
	const FDRItemInstance SourceItem = Slots[SourceSlotIndex];
	
	if (!SourceItem.IsValid()
		|| SourceItem.Quantity < RequestedQuantity
		|| DestinationInventory->GetAddableQuantity(SourceItem.Definition) < RequestedQuantity)
	{
		return 0;
	}
	
	if (SourceItem.RuntimeState.IsValid()
		&& RequestedQuantity != SourceItem.Quantity)
	{
		return 0;
	}
	
	// 기존 Item의 정보 복사
	FDRItemInstance TransferredItem = SourceItem;
	TransferredItem.Quantity = RequestedQuantity;
	
	// 새로운 InstanceId 발급
	if (RequestedQuantity < SourceItem.Quantity)
	{
		TransferredItem.InstanceId = FGuid::NewGuid();
	}
	
	if (!DestinationInventory->TryAddItemInstance(TransferredItem))
	{
		return 0;
	}
	
	RemoveFromSlotInternal(SourceSlotIndex, RequestedQuantity);

	HandleInventoryChangedOnServer();
	
	return RequestedQuantity;	
}

void UDRInventoryComponent::RequestSwapSlots(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
	if (!IsValidSlotIndex(SourceSlotIndex)
		|| !IsValidSlotIndex(TargetSlotIndex)
		|| SourceSlotIndex == TargetSlotIndex
		|| IsSlotLocked(SourceSlotIndex)
		|| IsSlotLocked(TargetSlotIndex))
	{
		return;
	}
	
	const FGuid ExpectedSourceInstanceId = GetInstanceIdAtSlot(SourceSlotIndex);
	const FGuid ExpectedTargetInstanceId = GetInstanceIdAtSlot(TargetSlotIndex);
	
	if (!ExpectedSourceInstanceId.IsValid())
	{
		return;
	}
	
	// 서버인 경우 즉시 교체를 시도한다.
	if (HasInventoryAuthority())
	{
		SwapSlotsInternal(SourceSlotIndex, TargetSlotIndex);
		return;
	}
	
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	
	if (IsValid(PlayerController)
		&& PlayerController->IsLocalController())
	{
		ServerRequestSwapSlots(SourceSlotIndex, TargetSlotIndex
			, ExpectedSourceInstanceId, ExpectedTargetInstanceId);
	}	
}

void UDRInventoryComponent::ServerRequestSwapSlots_Implementation(int32 SourceSlotIndex, int32 TargetSlotIndex,
	FGuid ExpectedSourceInstanceId, FGuid ExpectedTargetInstanceId)
{
	if (!IsValidSlotIndex(SourceSlotIndex)
		|| !IsValidSlotIndex(TargetSlotIndex)
		|| SourceSlotIndex == TargetSlotIndex
		|| IsSlotLocked(SourceSlotIndex)
		|| IsSlotLocked(TargetSlotIndex)
		|| GetInstanceIdAtSlot(SourceSlotIndex) != ExpectedSourceInstanceId
		|| GetInstanceIdAtSlot(TargetSlotIndex) != ExpectedTargetInstanceId)
	{
		return;
	}
	
	SwapSlotsInternal(SourceSlotIndex, TargetSlotIndex);
}

bool UDRInventoryComponent::CanAddItem(UDRItemDefinition* Definition, int32 Quantity) const
{
	return IsValid(Definition) 
	&& Quantity > 0 
	&& GetAddableQuantity(Definition) >= Quantity;
}

bool UDRInventoryComponent::CanAddItemInstance(const FDRItemInstance& ItemInstance) const
{
	if (!ItemInstance.IsValid()
		|| ItemInstance.Quantity > GetMaxStackSize(ItemInstance.Definition)
		|| FindItemInstance(ItemInstance.InstanceId) != nullptr
		|| GetAddableQuantity(ItemInstance.Definition) < ItemInstance.Quantity)
	{
		return false;
	}
	
	/* RuntimeState는 개별 인스턴스 상태
	* 여러 개가 하나의 스택을 공유하면 각 아이템의 상태를 구분할 수 없으므로,
	* RuntimeState가 있는 아이템은 MaxStackSize가 1이어야 한다.
	*/
	return !ItemInstance.RuntimeState.IsValid() 
		|| GetMaxStackSize(ItemInstance.Definition) == 1;
}

int32 UDRInventoryComponent::GetAddableQuantity(UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return 0;
	}
	
	const int32 MaxStackSize = GetMaxStackSize(Definition);
	int32 AvailableQuantity = 0;
	
	// 기존 스택의 남은 공간 계산
	for (int32 SlotIndex = LockedSlotCount; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FDRItemInstance& ItemInstance = Slots[SlotIndex];
		
		if (!ItemInstance.IsValid())
		{
			AvailableQuantity += MaxStackSize;
			continue;
		}
		
		if (ItemInstance.Definition == Definition
			&& !ItemInstance.RuntimeState.IsValid())
		{
			AvailableQuantity += FMath::Max(0, MaxStackSize - ItemInstance.Quantity);
		}
	}
	
	return AvailableQuantity;
}

int32 UDRInventoryComponent::GetItemCount(const UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return 0;
	}
	
	int32 TotalQuantity = 0;
	
	for (const FDRItemInstance& ItemInstance : Slots)
	{
		if (ItemInstance.IsValid()
			&& ItemInstance.Definition == Definition)
		{
			TotalQuantity += ItemInstance.Quantity;
		}
	}
	
	return TotalQuantity;
}

const FDRItemInstance* UDRInventoryComponent::GetItemAtSlot(int32 SlotIndex) const
{
	if (!Slots.IsValidIndex(SlotIndex)
		|| !Slots[SlotIndex].IsValid())
	{
		return nullptr;
	}
	
	return &Slots[SlotIndex];
}

const FDRItemInstance* UDRInventoryComponent::FindItemInstance(FGuid InstanceId) const
{
	if (!InstanceId.IsValid())
	{
		return nullptr;
	}
	
	return Slots.FindByPredicate([InstanceId](const FDRItemInstance& ItemInstance)
	{
		return ItemInstance.IsValid() && ItemInstance.InstanceId == InstanceId;
	});
}

const FDRItemInstance* UDRInventoryComponent::FindFirstItemInstanceByDefinition(
	const UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return nullptr;
	}

	return Slots.FindByPredicate([Definition](const FDRItemInstance& ItemInstance)
	{
		return ItemInstance.IsValid() && ItemInstance.Definition == Definition;
	});
}

bool UDRInventoryComponent::GetSnowProjectileWeaponUpgradeLevel(
	const UDRProjectileWeaponItemDefinition* WeaponDefinition,
	FGameplayTag UpgradeTag,
	int32& OutLevel) const
{
	OutLevel = 0;

	if (!IsValid(WeaponDefinition)
		|| WeaponDefinition->ResourceType != EDRProjectileWeaponResourceType::SnowGauge
		|| !UpgradeTag.IsValid()
		|| !IsValid(WeaponDefinition->UpgradeProfile)
		|| WeaponDefinition->UpgradeProfile->FindStatUpgrade(UpgradeTag) == nullptr)
	{
		return false;
	}

	const FDRItemInstance* ItemInstance = FindFirstItemInstanceByDefinition(WeaponDefinition);
	const FDRSnowProjectileWeaponRuntimeState* WeaponState = ItemInstance != nullptr
		? ItemInstance->RuntimeState.GetPtr<FDRSnowProjectileWeaponRuntimeState>()
		: nullptr;

	if (WeaponState == nullptr)
	{
		return false;
	}

	OutLevel = WeaponState->GetUpgradeLevel(UpgradeTag);
	return true;
}

bool UDRInventoryComponent::TryUpgradeSnowProjectileWeapon(
	FGuid InstanceId, FGameplayTag UpgradeTag, int32 ExpectedLevel)
{
	const FDRItemInstance* ItemInstance = FindItemInstance(InstanceId);
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = ItemInstance != nullptr
		? Cast<UDRProjectileWeaponItemDefinition>(ItemInstance->Definition.Get()) : nullptr;
	if (!HasInventoryAuthority()
		|| !IsValid(WeaponDefinition)
		|| WeaponDefinition->ResourceType != EDRProjectileWeaponResourceType::SnowGauge
		|| !IsValid(WeaponDefinition->UpgradeProfile)
		|| !WeaponDefinition->UpgradeProfile->IsUsable()
		|| ExpectedLevel < 0 || ExpectedLevel == MAX_int32)
	{
		return false;
	}

	const FDRWeaponStatUpgradeData* UpgradeData = WeaponDefinition->UpgradeProfile->FindStatUpgrade(UpgradeTag);
	const FDRWeaponUpgradeLevelData* TargetLevelData = UpgradeData != nullptr
		? UpgradeData->FindLevelData(ExpectedLevel + 1)
		: nullptr;
	if (TargetLevelData == nullptr || ItemInstance == nullptr)
	{
		return false;
	}

	return ModifyItemInstance(InstanceId,
		[UpgradeTag, ExpectedLevel](FDRItemInstance& Candidate)
		{
			FDRSnowProjectileWeaponRuntimeState* WeaponState =
				Candidate.RuntimeState.GetMutablePtr<FDRSnowProjectileWeaponRuntimeState>();

			return WeaponState != nullptr
				&& WeaponState->GetUpgradeLevel(UpgradeTag) == ExpectedLevel
				&& WeaponState->SetUpgradeLevel(UpgradeTag, ExpectedLevel + 1);
		});
}

int32 UDRInventoryComponent::FindSlotIndex(FGuid InstanceId) const
{
	if (!InstanceId.IsValid())
	{
		return INDEX_NONE;
	}
	
	return Slots.IndexOfByPredicate([InstanceId](const FDRItemInstance& ItemInstance)
	{
		return ItemInstance.IsValid() && ItemInstance.InstanceId == InstanceId;
	});
}

bool UDRInventoryComponent::ModifyItemInstance(FGuid InstanceId, TFunctionRef<bool(FDRItemInstance&)> Modifier)
{
	if (!HasInventoryAuthority())
	{
		return false;
	}
	
	const int32 SlotIndex = FindSlotIndex(InstanceId);
	
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	
	FDRItemInstance Candidate = Slots[SlotIndex];
	const UDRItemDefinition* CachedDefinition = Candidate.Definition;
	int32 CachedQuantity = Candidate.Quantity;
	
	// RuntimeState의 수정만을 허용한다.
	if (!Modifier(Candidate)
		|| !Candidate.IsValid()
		|| Candidate.InstanceId != InstanceId
		|| Candidate.Definition != CachedDefinition
		|| Candidate.Quantity != CachedQuantity)
	{
		return false;
	}
	
	Slots[SlotIndex] = MoveTemp(Candidate);
	HandleInventoryChangedOnServer();
	return true;	
}

void UDRInventoryComponent::OnRep_Slots()
{
	BroadcastInventoryChanged();
}

void UDRInventoryComponent::ResetWeaponUpgrades()
{
	if (!HasInventoryAuthority())
	{
		return;
	}

	bool IsChanged = false;
	for (FDRItemInstance& Item : Slots)
	{
		FDRSnowProjectileWeaponRuntimeState* WeaponState =
			Item.RuntimeState.GetMutablePtr<FDRSnowProjectileWeaponRuntimeState>();
		if (WeaponState != nullptr && !WeaponState->UpgradeLevels.IsEmpty())
		{
			WeaponState->UpgradeLevels.Reset();
			IsChanged = true;
		}
	}

	if (IsChanged)
	{
		HandleInventoryChangedOnServer();
	}
}

void UDRInventoryComponent::ResetInventory()
{
	if (!HasInventoryAuthority())
	{
		return;
	}

	Slots.Reset();
	Slots.SetNum(MaxSlots);
	HandleInventoryChangedOnServer();
}

void UDRInventoryComponent::HandleInventoryChangedOnServer()
{
	BroadcastInventoryChanged();
	
	AActor* OwnerActor = GetOwner();
	
	if (!IsValid(OwnerActor)
		|| !OwnerActor->GetIsReplicated())
	{
		return;
	}
	
	// 휴면 상태 인벤토리 깨우기
	OwnerActor->FlushNetDormancy();
	OwnerActor->ForceNetUpdate();	
}

void UDRInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChangedDelegate.Broadcast();
}

bool UDRInventoryComponent::HasInventoryAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	
	return IsValid(OwnerActor) 
		&& OwnerActor->HasAuthority();
}

bool UDRInventoryComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex);
}

int32 UDRInventoryComponent::GetMaxStackSize(const UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return 1;
	}
	
	return FMath::Max(1, Definition->MaxStackSize);
}

int32 UDRInventoryComponent::FindFirstEmptyUnlockedSlot() const
{
	for (int32 SlotIndex = LockedSlotCount; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex].IsValid())
		{
			return SlotIndex;
		}
	}
	
	return INDEX_NONE;	
}

FGuid UDRInventoryComponent::GetInstanceIdAtSlot(int32 SlotIndex) const
{
	const FDRItemInstance* ItemInstance = GetItemAtSlot(SlotIndex);
	
	return ItemInstance ? ItemInstance->InstanceId : FGuid();
}

void UDRInventoryComponent::AddItemInternal(UDRItemDefinition* Definition, int32 Quantity)
{
	check(IsValid(Definition));
	check(Quantity > 0);
	check(CanAddItem(Definition, Quantity));
	
	const int32 MaxStackSize = GetMaxStackSize(Definition);
	
	int32 RemainingQuantity = Quantity;
	
	for (int32 SlotIndex = LockedSlotCount ; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FDRItemInstance& ItemInstance = Slots[SlotIndex];

		if (!ItemInstance.IsValid()
			|| ItemInstance.Definition != Definition 
			|| ItemInstance.RuntimeState.IsValid())
		{
			continue;
		}
		
		const int32 FreeQuantity = FMath::Max(0, MaxStackSize - ItemInstance.Quantity);
		const int32 AddedQuantity = FMath::Min(RemainingQuantity, FreeQuantity);
		
		ItemInstance.Quantity += AddedQuantity;
		RemainingQuantity -= AddedQuantity;
	}
	
	while (RemainingQuantity > 0)
	{
		const int32 EmptySlotIndex = FindFirstEmptyUnlockedSlot();
		
		check(EmptySlotIndex != INDEX_NONE);
		
		const int32 NewStackQuantity = FMath::Min(RemainingQuantity, MaxStackSize);
		
		Slots[EmptySlotIndex]= DRItemInstanceFactory::Create(Definition, NewStackQuantity);
		
		check(Slots[EmptySlotIndex].IsValid());
		
		RemainingQuantity -= NewStackQuantity;
	}
}

void UDRInventoryComponent::AddItemInstanceInternal(const FDRItemInstance& ItemInstance)
{
	check(ItemInstance.IsValid());
	
	int32 RemainingQuantity = ItemInstance.Quantity;
	const int32 MaxStackSize = GetMaxStackSize(ItemInstance.Definition);
	
	// RuntimeState가 없는 아이템
	if (!ItemInstance.RuntimeState.IsValid())
	{
		for (int32 SlotIndex = LockedSlotCount; SlotIndex < Slots.Num() && RemainingQuantity > 0; ++SlotIndex)
		{
			FDRItemInstance& ExistingItem = Slots[SlotIndex];
			
			if (!ExistingItem.IsValid()
				|| ExistingItem.Definition != ItemInstance.Definition
				|| ExistingItem.RuntimeState.IsValid())
			{
				continue;
			}
			
			const int32 AddedQuantity = FMath::Min(RemainingQuantity, MaxStackSize - ExistingItem.Quantity);
			
			if (AddedQuantity <= 0)
			{
				continue;
			}
			
			ExistingItem.Quantity += AddedQuantity;
			RemainingQuantity -= AddedQuantity;
		}
	}
	
	if (RemainingQuantity <= 0)
	{
		return;
	}
	
	const int32 EmptySlotIndex = FindFirstEmptyUnlockedSlot();
	
	// 추가 함수가 호출되기 이전에 추가 가능 여부 확인이 일어나므로
	// 빈 슬롯이 없어서는 안된다.
	check(EmptySlotIndex != INDEX_NONE);
	
	FDRItemInstance RemainingItem = ItemInstance;
	RemainingItem.Quantity = RemainingQuantity;
	
	Slots[EmptySlotIndex] = MoveTemp(RemainingItem);
}

void UDRInventoryComponent::RemoveFromSlotInternal(int32 SlotIndex, int32 Quantity)
{
	check(Slots.IsValidIndex(SlotIndex));
	check(Quantity > 0);
	check(Slots[SlotIndex].Quantity >= Quantity);
	
	FDRItemInstance& ItemInstance = Slots[SlotIndex];
	
	if (ItemInstance.Quantity == Quantity)
	{
		ItemInstance = FDRItemInstance();
		return;
	}
	
	ItemInstance.Quantity -= Quantity;	
}

bool UDRInventoryComponent::SwapSlotsInternal(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
	if (!HasInventoryAuthority()
		|| !IsValidSlotIndex(SourceSlotIndex)
		|| !IsValidSlotIndex(TargetSlotIndex)
		|| SourceSlotIndex == TargetSlotIndex
		|| IsSlotLocked(SourceSlotIndex)
		|| IsSlotLocked(TargetSlotIndex)
		|| !Slots[SourceSlotIndex].IsValid())
	{
		return false;
	}
	
	Slots.Swap(SourceSlotIndex, TargetSlotIndex);
	
	HandleInventoryChangedOnServer();
	return true;
}
