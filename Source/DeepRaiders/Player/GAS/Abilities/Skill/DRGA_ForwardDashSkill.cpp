#include "DRGA_ForwardDashSkill.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
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

	UDRCharacterMovementComponent* Movement =
		Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!IsValid(Movement)
		|| DashDistance <= KINDA_SMALL_NUMBER
		|| DashDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Root Motion으로 목표 위치까지 이동시켜 체공 시간이 대쉬 거리를 늘리지 않게 한다.
	Movement->ClearAirborneMomentumPreservation();
	const FVector TargetLocation =
		Character->GetActorLocation() + DashDirection.GetSafeNormal2D() * DashDistance;
	UAbilityTask_ApplyRootMotionMoveToForce* MoveTask =
		UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this,
			TEXT("ForwardDashMove"),
			TargetLocation,
			DashDuration,
			false,
			MOVE_Walking,
			true,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			FMath::Max(Movement->MaxWalkSpeed, 0.0f));
	if (!IsValid(MoveTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(this, DashDuration);
	if (!IsValid(EndTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EndTask->OnFinish.AddDynamic(this, &ThisClass::HandleDashFinished);
	MoveTask->ReadyForActivation();
	EndTask->ReadyForActivation();
}

void UDRGA_ForwardDashSkill::HandleDashFinished()
{
	if (IsActive())
	{
		EndAbility(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActorInfo(),
			GetCurrentActivationInfo(),
			true,
			false);
	}
}
