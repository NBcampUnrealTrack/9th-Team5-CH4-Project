// Fill out your copyright notice in the Description page of Project Settings.


#include "DRQuickSlotComponent.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

UDRQuickSlotComponent::UDRQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UDRQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();
	
	CacheInventoryComponent();
	
	if (HasQuickSlotAuthority())
	{
		QuickSlots.SetNum(FMath::Max(1, InitialSlotCount));
	}
	
	CachedSlotCount = QuickSlots.Num();
	
	RefreshHandedItem();
}

void UDRQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDRInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->OnInventoryChangedDelegate.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		Inventory->OnEntryDefinitionReplacedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryEntryDefinitionReplaced);
	}
	
	Super::EndPlay(EndPlayReason);
}

void UDRQuickSlotComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, QuickSlots);
	DOREPLIFETIME(ThisClass, SelectedSlotIndex);
}

bool UDRQuickSlotComponent::TryBindFirstEmptySlot(UDRItemDefinition* Definition)
{
	if (!HasQuickSlotAuthority()
	|| !IsValid(Definition)
	|| !CacheInventoryComponent())
	{
		return false;
	}
	
	// 이미 바인딩 된 Definition
	for (const FDRQuickSlotEntry& Slot : QuickSlots)
	{
		if (Slot.Definition == Definition)
		{
			return true;
		}
	}
	
	// 가장 앞의 슬롯에 바인딩
	// 서버 호출이므로 Request 없이 구현부 실행
	for (int32 Index = 0; Index < QuickSlots.Num(); ++Index)
	{
		if (!QuickSlots[Index].IsBound())
		{
			return BindSlotInternal(Index, Definition);
		}
	}
	
	return false;
}

void UDRQuickSlotComponent::RequestBindSlot(int32 SlotIndex, UDRItemDefinition* Definition)
{
	if (!QuickSlots.IsValidIndex(SlotIndex)
		|| !IsValid(Definition))
	{
		return;
	}
	
	if (HasQuickSlotAuthority())
	{
		BindSlotInternal(SlotIndex, Definition);
		
		return;
	}
	
	if (IsLocalPlayer())
	{
		ServerBindSlot(SlotIndex, Definition);
	}
}

void UDRQuickSlotComponent::RequestClearSlot(int32 SlotIndex)
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (HasQuickSlotAuthority())
	{
		ClearSlotInternal(SlotIndex);
		return;
	}

	if (IsLocalPlayer())
	{
		ServerClearSlot(SlotIndex);
	}
}

void UDRQuickSlotComponent::RequestSelectSlot(int32 SlotIndex)
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (HasQuickSlotAuthority())
	{
		SelectSlotInternal(SlotIndex);
		return;
	}

	if (IsLocalPlayer())
	{
		ServerSelectSlot(SlotIndex);
	}
}

bool UDRQuickSlotComponent::SetSlotCount(int32 NewSlotCount)
{
	if (!HasQuickSlotAuthority()
		|| NewSlotCount <= 0
		|| QuickSlots.Num() == NewSlotCount)
	{
		return false;
	}
	
	const int32 PreviousSlotCount = QuickSlots.Num();
	const int32 PreviousSelectedSlotIndex = SelectedSlotIndex;
	
	QuickSlots.SetNum(NewSlotCount);
	CachedSlotCount = NewSlotCount;
	
	if (!QuickSlots.IsValidIndex(SelectedSlotIndex))
	{
		SelectedSlotIndex = INDEX_NONE;
	}
	
	OnQuickSlotCountChangedDelegate.Broadcast(NewSlotCount);
	OnQuickSlotsChangedDelegate.Broadcast();
	
	// 현재로선 슬롯 수 감소로 선택되어 있던 SlotIndex가 Invalid된 경우
	if (PreviousSelectedSlotIndex != SelectedSlotIndex)
	{
		OnSelectedQuickSlotIndexChangedDelegate.Broadcast(PreviousSelectedSlotIndex, SelectedSlotIndex);
	}
	
	RefreshHandedItem();
	RequestReplicationUpdate();
	
	UE_LOG(LogTemp, Log, TEXT("[%s] Quick Slot count Chanaged : %d -> %d"), *GetName(), PreviousSlotCount, NewSlotCount);
	
	return true;	
}

bool UDRQuickSlotComponent::ReplaceBoundDefinition(
	UDRItemDefinition* SourceDefinition,
	UDRItemDefinition* TargetDefinition)
{
	if (!HasQuickSlotAuthority()
		|| !IsValid(SourceDefinition)
		|| !IsValid(TargetDefinition)
		|| SourceDefinition == TargetDefinition)
	{
		return false;
	}

	bool IsReplaced = false;

	for (FDRQuickSlotEntry& QuickSlot : QuickSlots)
	{
		if (QuickSlot.Definition == SourceDefinition)
		{
			QuickSlot.Definition = TargetDefinition;
			IsReplaced = true;
		}
	}

	if (!IsReplaced)
	{
		return false;
	}

	OnQuickSlotsChangedDelegate.Broadcast();
	RefreshHandedItem();
	RequestReplicationUpdate();
	return true;
}

bool UDRQuickSlotComponent::GetQuickSlot(int32 SlotIndex, FDRQuickSlotEntry& OutSlot) const
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		OutSlot = FDRQuickSlotEntry();
		return false;
	}
	
	OutSlot = QuickSlots[SlotIndex];
	return true;
}

bool UDRQuickSlotComponent::IsSlotBound(int32 SlotIndex) const
{
	return QuickSlots.IsValidIndex(SlotIndex)
		&& QuickSlots[SlotIndex].IsBound();
}

bool UDRQuickSlotComponent::IsSlotItemAvailable(int32 SlotIndex) const
{
	// 슬롯에 바인딩된 아이템을 인벤토리에 보유하고 있는지 확인
	return ResolveHandedItemDefinition(SlotIndex) != nullptr;
}

int32 UDRQuickSlotComponent::GetSlotItemCount(int32 SlotIndex) const
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		return 0;
	}
	
	const UDRItemDefinition* Definition = QuickSlots[SlotIndex].Definition.Get();
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	if (!IsValid(Definition)
		|| !IsValid(Inventory))
	{
		return 0;
	}
	
	return Inventory->GetItemCount(Definition);
}

void UDRQuickSlotComponent::ServerBindSlot_Implementation(int32 SlotIndex, UDRItemDefinition* Definition)
{
	BindSlotInternal(SlotIndex, Definition);
}

void UDRQuickSlotComponent::ServerClearSlot_Implementation(int32 SlotIndex)
{
	ClearSlotInternal(SlotIndex);
}

void UDRQuickSlotComponent::ServerSelectSlot_Implementation(int32 SlotIndex)
{
	SelectSlotInternal(SlotIndex);
}

void UDRQuickSlotComponent::OnRep_QuickSlots()
{
	const int32 NewSlotCount = QuickSlots.Num();
	
	if (CachedSlotCount != NewSlotCount)
	{
		CachedSlotCount = NewSlotCount;
		
		// UI에 퀵슬롯 개수 변경 알림
		OnQuickSlotCountChangedDelegate.Broadcast(NewSlotCount);
	}
	
	OnQuickSlotsChangedDelegate.Broadcast();
	
	RefreshHandedItem();
}

void UDRQuickSlotComponent::OnRep_SelectedSlotIndex(int32 PreviousSlotIndex)
{
	OnSelectedQuickSlotIndexChangedDelegate.Broadcast(PreviousSlotIndex, SelectedSlotIndex);
	
	RefreshHandedItem();
}

void UDRQuickSlotComponent::HandleInventoryChanged()
{
	// 바인딩된 ItemDefinition은 그대로여도
	// 실제 Inventory 내의 보유 정보가 변하는 경우
	OnQuickSlotsChangedDelegate.Broadcast();
	
	RefreshHandedItem();
}

void UDRQuickSlotComponent::HandleInventoryEntryDefinitionReplaced(
	UDRItemDefinition* SourceDefinition,
	UDRItemDefinition* TargetDefinition)
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();

	if (!IsValid(Inventory)
		|| Inventory->GetItemCount(SourceDefinition) > 0)
	{
		return;
	}

	ReplaceBoundDefinition(SourceDefinition, TargetDefinition);
}

bool UDRQuickSlotComponent::CacheInventoryComponent()
{
	// 이미 캐시된 경우 캐시된 InventoryComponent 반환
	// 캐시된 InventoryComponent가 없는 경우 Owner에게서 캐싱
	if (InventoryComponent.IsValid())
	{
		return true;
	}
	
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}
	
	UDRInventoryComponent* FoundInventory = OwnerActor->FindComponentByClass<UDRInventoryComponent>();
	if (!IsValid(FoundInventory))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] InventoryComponent was not found"), *GetName());
		
		return false;
	}
	
	// 일단 플레이어와 연결된 InventoryComponent가 1개라고 가정
	InventoryComponent = FoundInventory;
	
	FoundInventory->OnInventoryChangedDelegate.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	FoundInventory->OnEntryDefinitionReplacedDelegate.AddDynamic(
		this,
		&ThisClass::HandleInventoryEntryDefinitionReplaced);
	
	return true;	
}

bool UDRQuickSlotComponent::HasQuickSlotAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	
	return IsValid(OwnerActor)
		&& OwnerActor->HasAuthority();
}

bool UDRQuickSlotComponent::IsLocalPlayer() const
{
	const APlayerController* PlayerController =	Cast<APlayerController>(GetOwner());

	return IsValid(PlayerController) 
		&& PlayerController->IsLocalController();
}

bool UDRQuickSlotComponent::BindSlotInternal(int32 SlotIndex, UDRItemDefinition* Definition)
{
	if (!HasQuickSlotAuthority()
		|| !QuickSlots.IsValidIndex(SlotIndex)
		|| !IsValid(Definition)
		|| !CacheInventoryComponent())
	{
		return false;
	}
	
	UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	if (!IsValid(Inventory)
		|| Inventory->GetItemCount(Definition) <= 0)
	{
		return false;
	}
	
	FDRQuickSlotEntry& Slot = QuickSlots[SlotIndex];
	
	if (Slot.Definition == Definition)
	{
		return false;
	}
	
	Slot.Definition = Definition;
	OnQuickSlotsChangedDelegate.Broadcast();
	
	if (SelectedSlotIndex == SlotIndex)
	{
		RefreshHandedItem();
	}
	
	RequestReplicationUpdate();
	
	return true;
}

bool UDRQuickSlotComponent::ClearSlotInternal(int32 SlotIndex)
{
	if (!HasQuickSlotAuthority()
		|| !QuickSlots.IsValidIndex(SlotIndex)
		|| !QuickSlots[SlotIndex].IsBound())
	{
		return false;
	}
	
	QuickSlots[SlotIndex] = FDRQuickSlotEntry();
	
	OnQuickSlotsChangedDelegate.Broadcast();
	
	if (SelectedSlotIndex == SlotIndex)
	{
		RefreshHandedItem();
	}
	
	RequestReplicationUpdate();
	
	return true;
}

bool UDRQuickSlotComponent::SelectSlotInternal(int32 SlotIndex)
{
	if (!HasQuickSlotAuthority()
		|| !QuickSlots.IsValidIndex(SlotIndex)
		|| SelectedSlotIndex == SlotIndex)
	{
		return false;
	}
	
	const int32 PreviousSlotIndex =	SelectedSlotIndex;
	SelectedSlotIndex = SlotIndex;

	OnSelectedQuickSlotIndexChangedDelegate.Broadcast(PreviousSlotIndex, SelectedSlotIndex);
	
	RefreshHandedItem();
	RequestReplicationUpdate();
	
	return true;
}

UDRItemDefinition* UDRQuickSlotComponent::ResolveHandedItemDefinition(int32 SlotIndex) const
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}
	
	UDRItemDefinition* Definition = QuickSlots[SlotIndex].Definition.Get();
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	// 인벤토리 내에 보유하지 않은 경우 nullptr 반환
	if (!IsValid(Definition)
		|| !IsValid(Inventory)
		|| Inventory->GetItemCount(Definition) <= 0)
	{
		return nullptr;
	}
	
	return Definition;	
}

void UDRQuickSlotComponent::RefreshHandedItem()
{
	UDRItemDefinition* NewHandedItem = ResolveHandedItemDefinition(SelectedSlotIndex);
	
	// 이미 쥐고 있는 아이템과 동일한 Definition
	if (HeldItemDefinition == NewHandedItem)
	{
		return;
	}
	
	HeldItemDefinition = NewHandedItem;
	
	// 캐릭터 외형에 반영
	ApplySelectedItemToCharacter();
	
	OnSelectedQuickSlotItemChangedDelegate.Broadcast(HeldItemDefinition.Get());	
}

void UDRQuickSlotComponent::RequestReplicationUpdate() const
{
	AActor* OwnerActor = GetOwner();
	
	if (!IsValid(OwnerActor)
		|| !OwnerActor->GetIsReplicated())
	{
		return;
	}
	
	OwnerActor->FlushNetDormancy();
	OwnerActor->ForceNetUpdate();
}

void UDRQuickSlotComponent::ApplySelectedItemToCharacter()
{
	if (!HasQuickSlotAuthority())
	{
		return;
	}
	
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	ADRPlayerCharacter* Character = IsValid(PlayerController) 
		? Cast<ADRPlayerCharacter>(PlayerController->GetPawn()) : nullptr; 
	
	if (IsValid(Character))
	{
		Character->SetHeldItemDefinition(HeldItemDefinition);
	}
}
