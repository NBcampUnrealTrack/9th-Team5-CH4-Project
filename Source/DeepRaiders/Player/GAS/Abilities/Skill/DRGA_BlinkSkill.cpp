#include "DRGA_BlinkSkill.h"

#include "GameFramework/Controller.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

void UDRGA_BlinkSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	if (!IsValid(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector BlinkDirection = Character->GetLastMovementInputVector().GetSafeNormal2D();
	if (BlinkDirection.IsNearlyZero())
	{
		const AController* Controller = Character->GetController();
		if (Controller != nullptr)
		{
			BlinkDirection = Controller->GetControlRotation().Vector().GetSafeNormal2D();
		}
	}

	if (BlinkDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Destination = Character->GetActorLocation() + BlinkDirection * BlinkDistance;
	const FRotator CharacterRotation = Character->GetActorRotation();

	if (!Character->TeleportTo(Destination, CharacterRotation, true, false)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo)
		|| !Character->TeleportTo(Destination, CharacterRotation, false, false))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
