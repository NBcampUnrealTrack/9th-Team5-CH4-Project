#include "DRGA_RocketBoots.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRGA_RocketBoots::UDRGA_RocketBoots()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(DRGameplayTags::Ability_Perk_SuperJump_RocketBoots);
	SetAssetTags(AbilityTagContainer);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_BlinkRecovery);
}

bool UDRGA_RocketBoots::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(
		Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const ADRPlayerCharacter* Character = ActorInfo != nullptr
		? Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UDRCharacterMovementComponent* MovementComponent = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	float MoveSpeed = 0.f;
	return IsValid(MovementComponent)
		&& MovementComponent->IsFalling()
		&& MovementComponent->IsSuperJumpActive()
		&& MovementComponent->GetSuperJumpSequence() != ConsumedSuperJumpSequence
		&& ResolvePerkValues(ActorInfo, MoveSpeed);
}

void UDRGA_RocketBoots::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = ActorInfo != nullptr
		? Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	UDRCharacterMovementComponent* MovementComponent = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	float MoveSpeed = 0.f;
	if (!IsValid(Character)
		|| !IsValid(MovementComponent)
		|| !MovementComponent->IsSuperJumpActive()
		|| MovementComponent->GetSuperJumpSequence() == ConsumedSuperJumpSequence
		|| !ResolvePerkValues(ActorInfo, MoveSpeed)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector DashDirection = Character->GetSkillMovementDirection();
	DashDirection.Z = 0.f;
	if (DashDirection.IsNearlyZero())
	{
		DashDirection = Character->GetActorForwardVector();
		DashDirection.Z = 0.f;
	}

	if (!MovementComponent->PerformHorizontalAirDash(DashDirection, MoveSpeed))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ConsumedSuperJumpSequence = MovementComponent->GetSuperJumpSequence();

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UDRGA_RocketBoots::ResolvePerkValues(
	const FGameplayAbilityActorInfo* ActorInfo,
	float& OutMoveSpeed) const
{
	OutMoveSpeed = 0.f;
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;
	if (!IsValid(PerkComponent))
	{
		return false;
	}

	OutMoveSpeed = PerkComponent->GetSkillEffectValue(
		DRGameplayTags::Ability_Skill_SuperJump,
		EDRSkillEffectTrigger::OnSkillCommitted,
		DRGameplayTags::Data_Perk_SuperJump_RocketBoots_MoveSpeed);
	return OutMoveSpeed > 0.f;
}
