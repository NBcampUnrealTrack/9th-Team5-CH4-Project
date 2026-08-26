#include "DRGA_ForwardDashSkill.h"

#include "GameFramework/Controller.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

void UDRGA_ForwardDashSkill::ActivateAbility(
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

	FVector DashDirection = Character->GetLastMovementInputVector().GetSafeNormal2D();
	if (DashDirection.IsNearlyZero())
	{
		const AController* Controller = Character->GetController();
		if (Controller != nullptr)
		{
			DashDirection = Controller->GetControlRotation().Vector().GetSafeNormal2D();
		}
	}

	if (DashDirection.IsNearlyZero() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Character->LaunchCharacter(DashDirection * DashVelocity, true, false);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
