// Fill out your copyright notice in the Description page of Project Settings.


#include "DRQuickSlotComponent.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Inventory/DRInventoryTypes.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Net/UnrealNetwork.h"

UDRQuickSlotComponent::UDRQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UDRQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();
	
	CachedInventoryComponent();
	
	CachedSlotCount = GetSlotCount();
	CachedSelectedSlotIndex = GetSelectedSlotIndex();
	
	if (HasQuickSlotAuthority())
	{
		EnsureValidSelection();
	}
	
	RefreshDerivedState();
}

void UDRQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDRInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->OnInventoryChangedDelegate.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		GrantedHandles.TakeFromAbilitySystem(ASC);	
	}
	
	Super::EndPlay(EndPlayReason);
}

void UDRQuickSlotComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, SelectedInstanceId);
}

void UDRQuickSlotComponent::RequestSelectSlot(int32 SlotIndex)
{
	if (IsQuickSlotSelectionLocked()
		|| !CachedInventoryComponent())
	{
		return;
	}

	const FDRItemInstance* ItemInstance = InventoryComponent->GetItemAtSlot(SlotIndex);
	
	if (!ItemInstance)
	{
		return;
	}
	
	if (HasQuickSlotAuthority())
	{
		SelectSlotInternal(SlotIndex, ItemInstance->InstanceId);
		return;
	}

	if (IsLocalPlayer())
	{
		ServerSelectSlot(SlotIndex, ItemInstance->InstanceId);
	}
}

void UDRQuickSlotComponent::ServerSelectSlot_Implementation(int32 SlotIndex, FGuid ExpectedInstanceId)
{
	SelectSlotInternal(SlotIndex, ExpectedInstanceId);
}

int32 UDRQuickSlotComponent::GetSlotCount() const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	return IsValid(Inventory) ? Inventory->GetMaxSlots() : 0;	
}

int32 UDRQuickSlotComponent::GetSelectedSlotIndex() const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	// 퀵슬롯과 인벤토리의 슬롯 인덱스가 1:1로 매칭됨.
	return IsValid(Inventory) ? Inventory->FindSlotIndex(SelectedInstanceId) : INDEX_NONE;	
}

bool UDRQuickSlotComponent::CanRequestLocalWeaponShot(const FGuid& WeaponInstanceId) const
{
	if (!IsLocalPlayer()
		|| !WeaponInstanceId.IsValid())
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	const double* NextFireTime = LocalNextWeaponFireTime.Find(WeaponInstanceId);

	return NextFireTime == nullptr || World->GetTimeSeconds() + KINDA_SMALL_NUMBER >= *NextFireTime;
}

void UDRQuickSlotComponent::RecordLocalWeaponShot(const FGuid& WeaponInstanceId, float FireInterval)
{
	if (!IsLocalPlayer()
		|| !WeaponInstanceId.IsValid()
		|| FireInterval <= 0.0f)
	{
		return;
	}

	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();

	// 만료된 다른 무기 기록도 함께 정리한다.
	for (auto Iterator = LocalNextWeaponFireTime.CreateIterator();
		Iterator;
		++Iterator)
	{
		if (Iterator.Value() <= CurrentTime)
		{
			Iterator.RemoveCurrent();
		}
	}

	LocalNextWeaponFireTime.FindOrAdd(WeaponInstanceId) = CurrentTime + FireInterval;
}

bool UDRQuickSlotComponent::GetQuickSlot(int32 SlotIndex, FDRItemInstance& OutItemInstance) const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	const FDRItemInstance* ItemInstance = IsValid(Inventory) ? Inventory->GetItemAtSlot(SlotIndex) : nullptr;

	if (!ItemInstance)
	{
		OutItemInstance = FDRItemInstance();
		return false;
	}
	
	OutItemInstance = *ItemInstance;
	return true;
	
}

bool UDRQuickSlotComponent::IsSlotBound(int32 SlotIndex) const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	return IsValid(Inventory)
		&& Inventory->GetItemAtSlot(SlotIndex) != nullptr;
}

bool UDRQuickSlotComponent::IsSlotItemAvailable(int32 SlotIndex) const
{
	return IsSlotBound(SlotIndex);
}

int32 UDRQuickSlotComponent::GetSlotItemCount(int32 SlotIndex) const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	const FDRItemInstance* ItemInstance = IsValid(Inventory) ? Inventory->GetItemAtSlot(SlotIndex) : nullptr;
	
	return ItemInstance ? ItemInstance->Quantity : 0;
}

void UDRQuickSlotComponent::OnRep_SelectedInstanceId()
{
	RefreshDerivedState();
}

void UDRQuickSlotComponent::HandleInventoryChanged()
{
	bool bSelectionChanged = false;
	
	if (HasQuickSlotAuthority())
	{
		bSelectionChanged = EnsureValidSelection();
	}
	
	RefreshDerivedState();
	
	if (bSelectionChanged)
	{
		RequestReplicationUpdate();
	}
}

bool UDRQuickSlotComponent::CachedInventoryComponent()
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

bool UDRQuickSlotComponent::SelectSlotInternal(int32 SlotIndex, FGuid ExpectedInstanceId)
{
	if (!HasQuickSlotAuthority()
		|| IsQuickSlotSelectionLocked()
		|| !CachedInventoryComponent())
	{
		return false;
	}
	
	const FDRItemInstance* ItemInstance = InventoryComponent->GetItemAtSlot(SlotIndex);
	
	if (!ItemInstance
		|| ItemInstance->InstanceId != ExpectedInstanceId
		|| ItemInstance->InstanceId == SelectedInstanceId)
	{
		return false;
	}
	
	SelectedInstanceId = ItemInstance->InstanceId;
	
	RefreshDerivedState();
	RequestReplicationUpdate();
	
	return true;	
}

bool UDRQuickSlotComponent::EnsureValidSelection()
{
	UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	if (!IsValid(Inventory))
	{
		return false;
	}
	
	if (SelectedInstanceId.IsValid()
		&& Inventory->FindItemInstance(SelectedInstanceId))
	{
		return false;
	}
	
	FGuid FallbackInstanceId;
	
	const FDRItemInstance* DefaultItem = Inventory->GetItemAtSlot(DRInventorySlots::DefaultWeapon);
	
	// 기본 무기가 있는 경우, 현재 장착 중인 무기가 제거될 때 자동으로 기본 무기를 들게 한다.
	if (DefaultItem)
	{
		FallbackInstanceId = DefaultItem->InstanceId;
	}
	
	if (FallbackInstanceId == SelectedInstanceId)
	{
		return false;
	}
	
	SelectedInstanceId = FallbackInstanceId;
	
	return true;	
}

const FDRItemInstance* UDRQuickSlotComponent::ResolveSelectedItem() const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	return IsValid(Inventory) ? Inventory->FindItemInstance(SelectedInstanceId) : nullptr;
}

void UDRQuickSlotComponent::RefreshDerivedState()
{
	const int32 NewSlotCount = GetSlotCount();
	const int32 NewSelectedSlotIndex = GetSelectedSlotIndex();
	
	if (NewSlotCount != CachedSlotCount)
	{
		CachedSlotCount = NewSlotCount;
		
		OnQuickSlotCountChangedDelegate.Broadcast(NewSlotCount);
	}
	
	OnQuickSlotsChangedDelegate.Broadcast();
	
	if (NewSelectedSlotIndex != CachedSelectedSlotIndex)
	{
		const int32 PreviousSlotIndex = CachedSelectedSlotIndex;
		
		CachedSelectedSlotIndex = NewSelectedSlotIndex;
		
		OnSelectedQuickSlotIndexChangedDelegate.Broadcast(PreviousSlotIndex, NewSelectedSlotIndex);
	}
	
	RefreshHeldItem();
}

void UDRQuickSlotComponent::RefreshHeldItem()
{
	const FDRItemInstance* SelectedItem = ResolveSelectedItem();
	
	UDRItemDefinition* NewDefinition = SelectedItem ? SelectedItem->Definition.Get() : nullptr;
	const FGuid NewInstanceId = SelectedItem ? SelectedItem->InstanceId : FGuid();
	
	if (EquippedInstanceId == NewInstanceId
		&& HeldItemDefinition == NewDefinition)
	{
		return;
	}
	
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		GrantedHandles.TakeFromAbilitySystem(ASC);
		
		if (IsValid(NewDefinition)
			&& IsValid(NewDefinition->ItemAbilitySet))
		{
			NewDefinition->ItemAbilitySet->GiveToAbilitySystem(ASC, &GrantedHandles, NewDefinition);
		}
	}
	
	EquippedInstanceId = NewInstanceId;
	HeldItemDefinition = NewDefinition;
	
	ApplySelectedItemToCharacter();
	
	OnSelectedQuickSlotItemChangedDelegate.Broadcast(HeldItemDefinition);	
}

void UDRQuickSlotComponent::RefreshSelectedItem()
{
	RefreshHeldItem();
	ApplySelectedItemToCharacter();	
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

bool UDRQuickSlotComponent::IsQuickSlotSelectionLocked() const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

	return IsValid(ASC)	&& ASC->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Active);
}
