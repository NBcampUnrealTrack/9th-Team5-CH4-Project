#include "DRGA_ForwardDashSkill.h"

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

	const FVector DashDirection = Character->GetSkillMovementDirection();
	if (DashDirection.IsNearlyZero() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Character->LaunchCharacter(DashDirection * DashVelocity, true, false);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
