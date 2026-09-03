#include "DRGA_ForwardDashSkill.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"

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

	if (UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		// Dash가 새 공중 속도를 결정하므로 이전 그래플의 속도 상한은 LaunchCharacter 전에 제거한다.
		Movement->ClearAirborneMomentumPreservation();
	}
	
	Character->LaunchCharacter(DashDirection * DashVelocity, true, false);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
