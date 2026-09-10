#include "DRSkillComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "GameplayAbilitySpec.h"
#include "Net/UnrealNetwork.h"

UDRSkillComponent::UDRSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	EquippedSkills.SetNum(static_cast<int32>(EDRSkillSlot::Count));
	SkillAbilityHandles.SetNum(static_cast<int32>(EDRSkillSlot::Count));
}

void UDRSkillComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UDRSkillComponent, EquippedSkills, COND_OwnerOnly);
}

UDRSkillDefinition* UDRSkillComponent::GetCurrentSkill(
	EDRSkillSlot SkillSlot) const
{
	const int32 SlotIndex = GetSkillSlotIndex(SkillSlot);
	return EquippedSkills.IsValidIndex(SlotIndex)
		? EquippedSkills[SlotIndex]
		: nullptr;
}

bool UDRSkillComponent::CanEquipSkill(
	const UDRSkillDefinition* SkillDefinition) const
{
	return IsValid(SkillDefinition)
		&& SkillDefinition->SkillAbility
		&& GetSkillInputId(SkillDefinition->SkillSlot) != INDEX_NONE;
}

bool UDRSkillComponent::EquipSkill(UDRSkillDefinition* SkillDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !CanEquipSkill(SkillDefinition))
	{
		return false;
	}

	const EDRSkillSlot SkillSlot = SkillDefinition->SkillSlot;
	const int32 SlotIndex = GetSkillSlotIndex(SkillSlot);
	const int32 SkillInputId = GetSkillInputId(SkillSlot);
	FGameplayAbilitySpec NewAbilitySpec(SkillDefinition->SkillAbility, 1);
	NewAbilitySpec.SourceObject = SkillDefinition;
	NewAbilitySpec.InputID = SkillInputId;
	const FGameplayAbilitySpecHandle NewSkillHandle =
		AbilitySystemComponent->GiveAbility(NewAbilitySpec);

	if (!NewSkillHandle.IsValid())
	{
		return false;
	}

	RemovePreviousSkillAbility(AbilitySystemComponent, SlotIndex);

	SkillAbilityHandles[SlotIndex] = NewSkillHandle;
	EquippedSkills[SlotIndex] = SkillDefinition;
	PlayerState->ForceNetUpdate();
	OnSkillChanged.Broadcast();
	return true;
}

void UDRSkillComponent::GrantDefaultSkills()
{
	for (UDRSkillDefinition* SkillDefinition : DefaultSkills)
	{
		EquipSkill(SkillDefinition);
	}
}

void UDRSkillComponent::ResetSkills()
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent))
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < SkillAbilityHandles.Num(); ++SlotIndex)
	{
		const UDRSkillDefinition* SkillDefinition = EquippedSkills.IsValidIndex(SlotIndex)
			? EquippedSkills[SlotIndex]
			: nullptr;
		if (IsValid(SkillDefinition) && SkillDefinition->CooldownTag.IsValid())
		{
			FGameplayTagContainer CooldownTags(SkillDefinition->CooldownTag);
			AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(CooldownTags);
		}

		RemovePreviousSkillAbility(AbilitySystemComponent, SlotIndex);
	}

	EquippedSkills.Init(nullptr, static_cast<int32>(EDRSkillSlot::Count));
	PlayerState->ForceNetUpdate();
	OnSkillChanged.Broadcast();
}

void UDRSkillComponent::RemovePreviousSkillAbility(
	UAbilitySystemComponent* AbilitySystemComponent,
	int32 SlotIndex)
{
	if (!IsValid(AbilitySystemComponent)
		|| !SkillAbilityHandles.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FGameplayAbilitySpecHandle PreviousSkillHandle =
		SkillAbilityHandles[SlotIndex];

	if (PreviousSkillHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(PreviousSkillHandle);
		AbilitySystemComponent->ClearAbility(PreviousSkillHandle);
	}

	SkillAbilityHandles[SlotIndex] = FGameplayAbilitySpecHandle();
}

int32 UDRSkillComponent::GetSkillInputId(EDRSkillSlot SkillSlot)
{
	switch (SkillSlot)
	{
	case EDRSkillSlot::One:
		return static_cast<int32>(EDRAbilityInputId::Skill1);

	case EDRSkillSlot::Two:
		return static_cast<int32>(EDRAbilityInputId::Skill2);

	default:
		return INDEX_NONE;
	}
}

int32 UDRSkillComponent::GetSkillSlotIndex(EDRSkillSlot SkillSlot)
{
	const int32 SlotIndex = static_cast<int32>(SkillSlot);
	return SlotIndex >= 0
		&& SlotIndex < static_cast<int32>(EDRSkillSlot::Count)
		? SlotIndex
		: INDEX_NONE;
}

void UDRSkillComponent::OnRep_EquippedSkills()
{
	OnSkillChanged.Broadcast();
}
