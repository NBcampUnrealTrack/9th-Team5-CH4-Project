#include "DRPerkComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Item/GAS/DRItemAbilitySet.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "Net/UnrealNetwork.h"

UDRPerkComponent::UDRPerkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRPerkComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UDRPerkComponent,
		PerkEntries,
		COND_OwnerOnly);
}

int32 UDRPerkComponent::GetPerkCount(
	const UDRPerkDefinition* PerkDefinition) const
{
	int32 PerkCount = 0;

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		if (PerkEntry.PerkDefinition == PerkDefinition)
		{
			++PerkCount;
		}
	}

	return PerkCount;
}

bool UDRPerkComponent::CanAddPerk(
	const UDRPerkDefinition* PerkDefinition) const
{
	return IsValid(PerkDefinition)
		&& IsValid(PerkDefinition->ItemAbilitySet)
		&& PerkEntries.Num() < MaxPerkSlotCount;
}

bool UDRPerkComponent::AddPerk(UDRPerkDefinition* PerkDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(PerkDefinition)
		|| !IsValid(PerkDefinition->ItemAbilitySet))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][AddFailed] Player=%s Perk=%s Reason=InvalidStateOrDefinition"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	if (!CanAddPerk(PerkDefinition))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][AddFailed] Player=%s Perk=%s Reason=SlotsFull Total=%d/%d"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition),
			PerkEntries.Num(),
			MaxPerkSlotCount);
		return false;
	}

	FDRItemAbilitySet_GrantedHandles GrantedHandles;

	PerkDefinition->ItemAbilitySet->GiveToAbilitySystem(
		AbilitySystemComponent,
		&GrantedHandles,
		PerkDefinition);

	if (GrantedHandles.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][AbilitySetFailed] Player=%s Perk=%s Reason=NoGrantedHandles"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][AbilitySetApplied] Player=%s Perk=%s"),
		*GetNameSafe(PlayerState),
		*GetNameSafe(PerkDefinition));

	FDRPerkEntry& PerkEntry = PerkEntries.AddDefaulted_GetRef();
	PerkEntry.PerkDefinition = PerkDefinition;
	PerkEntry.GrantedHandles = MoveTemp(GrantedHandles);
	const int32 SlotIndex = PerkEntries.Num() - 1;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][SlotAdded] Player=%s Slot=%d Perk=%s DuplicateCount=%d Total=%d/%d"),
		*GetNameSafe(PlayerState),
		SlotIndex,
		*GetNameSafe(PerkDefinition),
		GetPerkCount(PerkDefinition),
		PerkEntries.Num(),
		MaxPerkSlotCount);

	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return true;
}

bool UDRPerkComponent::ResetPerks()
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent))
	{
		return false;
	}

	for (FDRPerkEntry& PerkEntry : PerkEntries)
	{
		PerkEntry.GrantedHandles.TakeFromAbilitySystem(
			AbilitySystemComponent);
	}

	PerkEntries.Reset();

	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return true;
}

void UDRPerkComponent::RequestResetPerks()
{
	ServerResetPerks();
}

void UDRPerkComponent::ServerResetPerks_Implementation()
{
	ResetPerks();
}

void UDRPerkComponent::OnRep_PerkEntries()
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][SlotsReplicated] Player=%s Total=%d/%d"),
		*GetNameSafe(GetOwner()),
		PerkEntries.Num(),
		MaxPerkSlotCount);

	for (int32 SlotIndex = 0;
		SlotIndex < PerkEntries.Num();
		++SlotIndex)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Perk][Slot] Player=%s Slot=%d Perk=%s"),
			*GetNameSafe(GetOwner()),
			SlotIndex,
			*GetNameSafe(PerkEntries[SlotIndex].PerkDefinition));
	}

	OnPerksChanged.Broadcast();
}
