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
	FHitResult HitResult;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Sweep the character's collision capsule so a blocking wall clamps the blink
	// to the first impact instead of allowing teleport destination adjustment past it.
	Character->SetActorLocation(Destination, true, &HitResult, ETeleportType::TeleportPhysics);
	Character->SetActorRotation(CharacterRotation, ETeleportType::TeleportPhysics);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
