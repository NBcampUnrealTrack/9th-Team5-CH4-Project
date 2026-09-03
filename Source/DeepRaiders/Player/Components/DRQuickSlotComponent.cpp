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
#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
#include "TimerManager.h"
#include "DeepRaiders/GAS/Effects/DRGE_QuickSlotActivationInterval.h"
#include "GameplayEffect.h"

UDRQuickSlotComponent::UDRQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UDRQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();
	
	CachedInventoryComponent();
	CacheAbilitySystemComponent();
	
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
	
	ClearDeferredHeldItemRefresh();
	ClearAuthorityQuickSlotActivationInterval();
	
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	
	if (!IsValid(ASC))
	{
		ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	}
		
	UnbindAbilitySystemComponent();
	
	if (IsValid(ASC))
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
	
	if (!ItemInstance
		|| ItemInstance->InstanceId == SelectedInstanceId)
	{
		return;
	}
	
	if (IsLocalPlayer())
	{
		StartLocalQuickSlotActivationInterval(*ItemInstance);
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

void UDRQuickSlotComponent::RequestSelectAdjacentSlot(int32 Direction)
{
	if (Direction == 0 
		|| !CachedInventoryComponent())
	{
		return;
	}
	
	const int32 TargetSlotIndex = FindAdjacentAvailableSlotIndex(GetSelectedSlotIndex(), Direction);
	
	if (TargetSlotIndex == INDEX_NONE)
	{
		return;
	}
	
	RequestSelectSlot(TargetSlotIndex);
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
	if (bHeldItemRefreshDeferred)
	{
		return CachedSelectedSlotIndex;
	}
	
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
	CacheAbilitySystemComponent();

	if (ShouldDeferHeldItemRefresh())
	{
		bHeldItemRefreshDeferred = true;
		RefreshQuickSlotCollectionState();
		return;
	}

	ClearDeferredHeldItemRefresh();
	
	RefreshDerivedState();
}

void UDRQuickSlotComponent::HandleInventoryChanged()
{
	CacheAbilitySystemComponent();

	/*
	 * 마지막 스택이 제거되어 선택 아이템이 사라졌더라도 활성 Ability가 끝날 때까지
	 * 선택 표시, 손 외형, AbilitySet은 기존 상태로 유지한다.
	 */
	if (ShouldDeferHeldItemRefresh())
	{
		bHeldItemRefreshDeferred = true;

		// 슬롯 내용과 수량은 즉시 갱신하되 선택 아이템 상태는 건드리지 않는다.
		RefreshQuickSlotCollectionState();
		return;
	}
	
	ClearDeferredHeldItemRefresh();	
	
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
	
	const bool bHadPreviousSelection = SelectedInstanceId.IsValid();
	SelectedInstanceId = ItemInstance->InstanceId;
	
	if (bHadPreviousSelection)
	{
		ApplyAuthorityQuickSlotActivationInterval(*ItemInstance);
	}
	else
	{
		ClearAuthorityQuickSlotActivationInterval();
	}
	
	RefreshDerivedState();
	RequestReplicationUpdate();
	
	return true;	
}

int32 UDRQuickSlotComponent::FindAdjacentAvailableSlotIndex(int32 StartSlotIndex, int32 Direction)
{
	const int32 SlotCount = GetSlotCount();
	
	if (SlotCount <= 0
		|| Direction == 0)
	{
		return INDEX_NONE;
	}
	
	const int32 Step = Direction > 0 ? 1 : -1;
	const bool bHasValidStartIndex = StartSlotIndex >= 0 && StartSlotIndex < SlotCount;
	const int32 SearchStartIndex = bHasValidStartIndex ? StartSlotIndex : (Step > 0 ? SlotCount -1 : 0);
	
	for (int32 Offset = 1; Offset <= SlotCount; ++Offset)
	{
		int32 CandidateSlotIndex = (SearchStartIndex + Step * Offset) % SlotCount;
		
		if (CandidateSlotIndex < 0)
		{
			CandidateSlotIndex += SlotCount;
		}
		
		if (CandidateSlotIndex == StartSlotIndex)
		{
			continue;
		}
		
		if (IsSlotItemAvailable(CandidateSlotIndex))
		{
			return CandidateSlotIndex;
		}
	}
	
	return INDEX_NONE;
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
	
	const FGuid PreviousInstanceId = SelectedInstanceId;
	const FDRItemInstance* DefaultItem = Inventory->GetItemAtSlot(DRInventorySlots::DefaultWeapon);
	// 기본 무기가 있는 경우, 현재 장착 중인 무기가 제거될 때 자동으로 기본 무기를 들게 한다.
	FGuid FallbackInstanceId = DefaultItem ? DefaultItem->InstanceId : FGuid();
	
	if (FallbackInstanceId == SelectedInstanceId)
	{
		return false;
	}
	
	SelectedInstanceId = FallbackInstanceId;
	
	if (PreviousInstanceId.IsValid()
		&& DefaultItem)
	{
		ApplyAuthorityQuickSlotActivationInterval(*DefaultItem);
	}
	else
	{
		ClearAuthorityQuickSlotActivationInterval();
	}
	
	return true;	
}

const FDRItemInstance* UDRQuickSlotComponent::ResolveSelectedItem() const
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	return IsValid(Inventory) ? Inventory->FindItemInstance(SelectedInstanceId) : nullptr;
}

void UDRQuickSlotComponent::RefreshDerivedState()
{
	RefreshQuickSlotCollectionState();
	RefreshSelectedItemState();
}

void UDRQuickSlotComponent::RefreshHeldItem()
{
	CacheAbilitySystemComponent();
	
	const FDRItemInstance* SelectedItem = ResolveSelectedItem();
	
	UDRItemDefinition* NewDefinition = SelectedItem ? SelectedItem->Definition.Get() : nullptr;
	const FGuid NewInstanceId = SelectedItem ? SelectedItem->InstanceId : FGuid();
	
	if (EquippedInstanceId == NewInstanceId
		&& HeldItemDefinition == NewDefinition)
	{
		return;
	}
	
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
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
	CacheAbilitySystemComponent();

	if (ShouldDeferHeldItemRefresh())
	{
		bHeldItemRefreshDeferred = true;
		RefreshQuickSlotCollectionState();
		return;
	}

	ClearDeferredHeldItemRefresh();
	
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

	return IsValid(ASC) &&
		ASC->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Active);
}

bool UDRQuickSlotComponent::CacheAbilitySystemComponent()
{
	UAbilitySystemComponent* FoundASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	
	if (!IsValid(FoundASC))
	{
		return false;
	}
	
	if (AbilitySystemComponent.Get() != FoundASC)
	{
		UnbindAbilitySystemComponent();
		AbilitySystemComponent = FoundASC;
	}
	
	if (!AbilityEndedDelegateHandle.IsValid())
	{
		AbilityEndedDelegateHandle = FoundASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	}
	
	if (!QuickSlotActivationIntervalTagChangedDelegateHandle.IsValid())
	{
		QuickSlotActivationIntervalTagChangedDelegateHandle = FoundASC->RegisterGameplayTagEvent(
			DRGameplayTags::State_QuickSlot_ActivationInterval, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &ThisClass::HandleQuickSlotActivationIntervalTagChanged);
	}
	
	return true;
}

void UDRQuickSlotComponent::UnbindAbilitySystemComponent()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		if (AbilityEndedDelegateHandle.IsValid())
		{
			ASC->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
		}
		
		if (QuickSlotActivationIntervalTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(DRGameplayTags::State_QuickSlot_ActivationInterval,
				EGameplayTagEventType::NewOrRemoved).Remove(QuickSlotActivationIntervalTagChangedDelegateHandle);
		}
	}

	AbilityEndedDelegateHandle.Reset();
	QuickSlotActivationIntervalTagChangedDelegateHandle.Reset();
	AbilitySystemComponent.Reset();
}

bool UDRQuickSlotComponent::HasActiveHeldItemAbility() const
{
	if (!IsValid(HeldItemDefinition))
	{
		return false;
	}
	
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	
	if (!IsValid(ASC))
	{
		ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	}
	
	if (!IsValid(ASC))
	{
		return false;
	}
	
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive()
			&& Spec.SourceObject.Get() == HeldItemDefinition)
		{
			return true;
		}
	}
	
	return false;		
}

bool UDRQuickSlotComponent::ShouldDeferHeldItemRefresh() const
{
	if (!IsValid(HeldItemDefinition)
		|| HeldItemDefinition->AbilityLifetimePolicy != EDRItemAbilityLifetimePolicy::KeepWhileActive)
	{
		return false;
	}	
	
	const FDRItemInstance* SelectedItem = ResolveSelectedItem();
	const FGuid NewInstanceId = SelectedItem ? SelectedItem->InstanceId : FGuid();
	
	if (NewInstanceId == EquippedInstanceId)
	{
		return false;
	}
	
	return HasActiveHeldItemAbility();
}

void UDRQuickSlotComponent::QueueDeferredHeldItemRefresh()
{
	if (!bHeldItemRefreshDeferred 
		|| DeferredHeldItemRefreshTimerHandle.IsValid())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	DeferredHeldItemRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::ApplyDeferredHeldItemRefresh));
}

void UDRQuickSlotComponent::ApplyDeferredHeldItemRefresh()
{
	DeferredHeldItemRefreshTimerHandle.Invalidate();
	
	if (!bHeldItemRefreshDeferred
		||HasActiveHeldItemAbility())
	{
		return;
	}
	
	/*
	 * 클라이언트에서 마지막 스택이 먼저 사라졌다면 서버가 확정한 SelectedInstanceId의 OnRep를 기다린다.
	 */
	if (!HasQuickSlotAuthority()
		&& ResolveSelectedItem() == nullptr)
	{
		return;
	}
	
	bool bSelectionChanged = false;
	
	if (HasQuickSlotAuthority())
	{
		bSelectionChanged = EnsureValidSelection();
	}
	
	ClearDeferredHeldItemRefresh();
	RefreshDerivedState();
	
	if (bSelectionChanged)
	{
		RequestReplicationUpdate();
	}
}

void UDRQuickSlotComponent::ClearDeferredHeldItemRefresh()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredHeldItemRefreshTimerHandle);
	}
	
	DeferredHeldItemRefreshTimerHandle.Invalidate();
	bHeldItemRefreshDeferred = false;
}

void UDRQuickSlotComponent::RefreshQuickSlotCollectionState()
{
	const int32 NewSlotCount = GetSlotCount();
	
	if (NewSlotCount != CachedSlotCount)
	{
		CachedSlotCount = NewSlotCount;
		OnQuickSlotCountChangedDelegate.Broadcast(NewSlotCount);
	}
	
	OnQuickSlotsChangedDelegate.Broadcast();
}

void UDRQuickSlotComponent::RefreshSelectedItemState()
{
	const int32 NewSelectedSlotIndex = GetSelectedSlotIndex();
	if (NewSelectedSlotIndex != CachedSelectedSlotIndex)
	{
		const int32 PreviousSlotIndex = CachedSelectedSlotIndex;
		
		CachedSelectedSlotIndex = NewSelectedSlotIndex;
		
		OnSelectedQuickSlotIndexChangedDelegate.Broadcast(PreviousSlotIndex, NewSelectedSlotIndex);
	}
	
	RefreshHeldItem();
}

void UDRQuickSlotComponent::HandleAbilityEnded(const FAbilityEndedData& AbilityEndedData)
{
	if (!bHeldItemRefreshDeferred 
		|| HasActiveHeldItemAbility())
	{
		return;
	}

	/*
	 * GAS의 Ability 종료 Delegate 실행 중에는 Ability Spec 목록이 정리되는 중일 수 있다.
	 * AbilitySet 회수와 다음 아이템 장착은 다음 틱에 처리한다.
	 */
	QueueDeferredHeldItemRefresh();
}

bool UDRQuickSlotComponent::IsQuickSlotActivationIntervalActive() const
{
	const double CurrentTime = GetQuickSlotActivationIntervalTime();
	const bool bHasLocalPrediction = LocalActivationIntervalDuration > KINDA_SMALL_NUMBER 
		&& CurrentTime + KINDA_SMALL_NUMBER < LocalActivationIntervalEndTime;
	
	if (bHasLocalPrediction)
	{
		return true;
	}
	
	float Remaining = 0.0f;
	float Duration = 0.0f;
	return QueryAuthorityQuickSlotActivationInterval(Remaining, Duration);
}

bool UDRQuickSlotComponent::GetQuickSlotActivationIntervalState(int32 SlotIndex, float& OutProgress) const
{
	OutProgress = 1.f;
	
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	if (!IsValid(Inventory))
	{
		return false;
	}
	
	const double CurrentTime = GetQuickSlotActivationIntervalTime();
	const bool bHasLocalPrediction = LocalActivationIntervalDuration > KINDA_SMALL_NUMBER
		&& CurrentTime + KINDA_SMALL_NUMBER < LocalActivationIntervalEndTime;
	
	if (bHasLocalPrediction)
	{
		const int32 PredictedSlotIndex = Inventory->FindSlotIndex(LocalActivationIntervalInstanceId);
		
		if (PredictedSlotIndex != SlotIndex)
		{
			return false;
		}
		
		const double ElapsedTime = CurrentTime - LocalActivationIntervalStartTime;
		OutProgress = FMath::Clamp(static_cast<float>(ElapsedTime / LocalActivationIntervalDuration), 0.0f, 1.0f);
		return true;
	}
	
	float Remaining = 0.0f;
	float Duration = 0.0f;
	if (!QueryAuthorityQuickSlotActivationInterval(Remaining, Duration)
		|| GetSelectedSlotIndex() != SlotIndex)
	{
		return false;
	}
	
	OutProgress = Duration > KINDA_SMALL_NUMBER ? 1.0f - FMath::Clamp(Remaining / Duration, 0.0f, 1.0f) : 1.0f;
	
	return true;
}

void UDRQuickSlotComponent::StartLocalQuickSlotActivationInterval(const FDRItemInstance& ItemInstance)
{
	if (!IsLocalPlayer()
		|| !ItemInstance.IsValid())
	{
		return;
	}
	
	const UDRItemDefinition* ItemDefinition = ItemInstance.Definition.Get();
	const float Duration = IsValid(ItemDefinition) ? FMath::Max(ItemDefinition->QuickSlotActivationInterval) : 0.0f;
	const double CurrentTime = GetQuickSlotActivationIntervalTime();
	
	LocalActivationIntervalInstanceId = ItemInstance.InstanceId;
	LocalActivationIntervalDuration = Duration;
	LocalActivationIntervalStartTime = CurrentTime;
	LocalActivationIntervalEndTime = CurrentTime + Duration;

	OnQuickSlotActivationIntervalChangedDelegate.Broadcast();
}

void UDRQuickSlotComponent::ApplyAuthorityQuickSlotActivationInterval(
	const FDRItemInstance& ItemInstance)
{
	if (!HasQuickSlotAuthority() 
		|| !ItemInstance.IsValid())
	{
		return;
	}

	ClearAuthorityQuickSlotActivationInterval();

	const UDRItemDefinition* ItemDefinition = ItemInstance.Definition.Get();
	const float Duration = IsValid(ItemDefinition) 
		? FMath::Max(0.0f, ItemDefinition->QuickSlotActivationInterval) : 0.0f;

	if (Duration <= KINDA_SMALL_NUMBER 
		|| !CacheAbilitySystemComponent())
	{
		return;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(ItemInstance.Definition.Get());

	FGameplayEffectSpecHandle EffectSpec = ASC->MakeOutgoingSpec(UDRGE_QuickSlotActivationInterval::StaticClass(),
		1.0f,EffectContext);

	if (!EffectSpec.IsValid())
	{
		return;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_QuickSlot_ActivationInterval_Duration,	Duration);

	AuthorityQuickSlotActivationIntervalEffectHandle =
		ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
}

void UDRQuickSlotComponent::ClearAuthorityQuickSlotActivationInterval()
{
	if (!HasQuickSlotAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC))
	{
		ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	}

	if (IsValid(ASC) && AuthorityQuickSlotActivationIntervalEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(
			AuthorityQuickSlotActivationIntervalEffectHandle);
	}

	AuthorityQuickSlotActivationIntervalEffectHandle.Invalidate();
}

bool UDRQuickSlotComponent::QueryAuthorityQuickSlotActivationInterval(
	float& OutRemaining,
	float& OutDuration) const
{
	OutRemaining = 0.0f;
	OutDuration = 0.0f;

	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC))
	{
		ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	}

	if (!IsValid(ASC)
		|| !ASC->HasMatchingGameplayTag(DRGameplayTags::State_QuickSlot_ActivationInterval))
	{
		return false;
	}

	FGameplayTagContainer IntervalTags;
	IntervalTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(IntervalTags);
	const TArray<TPair<float, float>> Effects = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);

	for (const TPair<float, float>& Effect : Effects)
	{
		if (Effect.Key > OutRemaining)
		{
			OutRemaining = Effect.Key;
			OutDuration = Effect.Value;
		}
	}

	return OutRemaining > KINDA_SMALL_NUMBER;
}

double UDRQuickSlotComponent::GetQuickSlotActivationIntervalTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

void UDRQuickSlotComponent::HandleQuickSlotActivationIntervalTagChanged(
	FGameplayTag,
	int32)
{
	OnQuickSlotActivationIntervalChangedDelegate.Broadcast();
}











