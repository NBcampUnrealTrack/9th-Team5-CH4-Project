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
		TestPerkSlots,
		COND_OwnerOnly);
}

int32 UDRPerkComponent::GetTestPerkCount(
	const UDRPerkDefinition* PerkDefinition) const
{
	int32 TestPerkCount = 0;

	for (const UDRPerkDefinition* TestPerk : TestPerkSlots)
	{
		if (TestPerk == PerkDefinition)
		{
			++TestPerkCount;
		}
	}

	return TestPerkCount;
}

bool UDRPerkComponent::CanAddTestPerk(
	const UDRPerkDefinition* PerkDefinition) const
{
	return IsValid(PerkDefinition)
		&& IsValid(PerkDefinition->ItemAbilitySet)
		&& TestPerkSlots.Num() < TestMaxPerkSlotCount;
}

bool UDRPerkComponent::AddTestPerk(UDRPerkDefinition* PerkDefinition)
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
			TEXT("[Perk][Test][AddFailed] Player=%s Perk=%s Reason=InvalidStateOrDefinition"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	if (!CanAddTestPerk(PerkDefinition))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][Test][AddFailed] Player=%s Perk=%s Reason=SlotsFull Total=%d/%d"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition),
			TestPerkSlots.Num(),
			TestMaxPerkSlotCount);
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
			TEXT("[Perk][Test][AbilitySetFailed] Player=%s Perk=%s Reason=NoGrantedHandles"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][Test][AbilitySetApplied] Player=%s Perk=%s"),
		*GetNameSafe(PlayerState),
		*GetNameSafe(PerkDefinition));

	const int32 SlotIndex = TestPerkSlots.Add(PerkDefinition);
	const bool IsSlotValid = TestPerkSlots.IsValidIndex(SlotIndex)
		&& TestPerkSlots[SlotIndex] == PerkDefinition;

	if (!IsSlotValid)
	{
		GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);

		if (TestPerkSlots.IsValidIndex(SlotIndex))
		{
			TestPerkSlots.RemoveAt(SlotIndex);
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("[Perk][Test][SlotAddFailed] Player=%s Slot=%d Perk=%s"),
			*GetNameSafe(PlayerState),
			SlotIndex,
			*GetNameSafe(PerkDefinition));
		return false;
	}

	const int32 HandleIndex = TestGrantedHandles.Add(MoveTemp(GrantedHandles));

	if (HandleIndex != SlotIndex)
	{
		if (TestGrantedHandles.IsValidIndex(HandleIndex))
		{
			TestGrantedHandles[HandleIndex].TakeFromAbilitySystem(
				AbilitySystemComponent);
			TestGrantedHandles.RemoveAt(HandleIndex);
		}

		TestPerkSlots.RemoveAt(SlotIndex);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("[Perk][Test][HandleStoreFailed] Player=%s Slot=%d Perk=%s"),
			*GetNameSafe(PlayerState),
			SlotIndex,
			*GetNameSafe(PerkDefinition));
		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][Test][SlotAdded] Player=%s Slot=%d Perk=%s IsValid=%d DuplicateCount=%d Total=%d/%d"),
		*GetNameSafe(PlayerState),
		SlotIndex,
		*GetNameSafe(PerkDefinition),
		IsSlotValid,
		GetTestPerkCount(PerkDefinition),
		TestPerkSlots.Num(),
		TestMaxPerkSlotCount);

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

	for (FDRItemAbilitySet_GrantedHandles& GrantedHandles
		: TestGrantedHandles)
	{
		GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	}

	TestGrantedHandles.Reset();
	TestPerkSlots.Reset();

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

void UDRPerkComponent::OnRep_TestPerkSlots()
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][Test][SlotsReplicated] Player=%s Total=%d/%d"),
		*GetNameSafe(GetOwner()),
		TestPerkSlots.Num(),
		TestMaxPerkSlotCount);

	for (int32 SlotIndex = 0;
		SlotIndex < TestPerkSlots.Num();
		++SlotIndex)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Perk][Test][Slot] Player=%s Slot=%d Perk=%s"),
			*GetNameSafe(GetOwner()),
			SlotIndex,
			*GetNameSafe(TestPerkSlots[SlotIndex]));
	}

	OnPerksChanged.Broadcast();
}
