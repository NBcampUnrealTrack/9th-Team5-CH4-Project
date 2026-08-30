#include "DRGA_SuperJumpSkill.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"

bool UDRGA_SuperJumpSkill::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (bAllowInAir)
	{
		return true;
	}

	const ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	const UCharacterMovementComponent* MovementComponent = IsValid(Character)
		? Character->GetCharacterMovement()
		: nullptr;

	return IsValid(MovementComponent) && MovementComponent->IsMovingOnGround();
}

void UDRGA_SuperJumpSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	if (!IsValid(Character) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UDRCharacterMovementComponent* MovementComponent =
		Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		MovementComponent->ActivateSuperJumpAirControl(AirControl);
	}

	Character->LaunchCharacter(FVector::UpVector * JumpVelocity, false, true);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
