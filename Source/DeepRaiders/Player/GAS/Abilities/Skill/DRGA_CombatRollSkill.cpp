#include "DRGA_CombatRollSkill.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

bool UDRGA_CombatRollSkill::CanActivateAbility(
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

	const ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	const UCharacterMovementComponent* MovementComponent = IsValid(Character)
		? Character->GetCharacterMovement()
		: nullptr;

	return IsValid(MovementComponent) && MovementComponent->IsMovingOnGround();
}

void UDRGA_CombatRollSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	UCharacterMovementComponent* MovementComponent = IsValid(Character)
		? Character->GetCharacterMovement()
		: nullptr;
	if (!IsValid(Character)
		|| !IsValid(MovementComponent)
		|| !IsValid(RollMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector RollDirection = ResolveRollDirection(Character);
	const FName RollSection = ResolveRollSection(Character, RollDirection);
	const int32 RollSectionIndex = RollMontage->GetSectionIndex(RollSection);
	if (RollDirection.IsNearlyZero()
		|| RollSectionIndex == INDEX_NONE)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float RollDuration = RollMontage->GetSectionLength(RollSectionIndex) / RollPlayRate;
	if (RollDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		RollMontage,
		RollPlayRate,
		RollSection,
		true);

	if (!IsValid(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector TargetLocation = Character->GetActorLocation() + RollDirection * RollDistance;
	UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this,
		TEXT("CombatRollMove"),
		TargetLocation,
		RollDuration,
		false,
		MOVE_Walking,
		true,
		nullptr,
		// 종료 순간 속도를 0으로 만들면 입력이 유지돼도 다시 가속해야 해
		// 구르기 뒤 이동이 끊긴다. 걷기 속도 범위에서만 관성을 이어간다.
		ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector,
		FMath::Max(MovementComponent->MaxWalkSpeed, 0.0f));

	if (!IsValid(MoveTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRollFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRollInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRollInterrupted);
	MoveTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

FVector UDRGA_CombatRollSkill::ResolveRollDirection(const ADRPlayerCharacter* Character) const
{
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const FVector InputDirection = Character->GetLastMovementInputVector().GetSafeNormal2D();
	return InputDirection.IsNearlyZero()
		? Character->GetActorForwardVector().GetSafeNormal2D()
		: InputDirection;
}

FName UDRGA_CombatRollSkill::ResolveRollSection(
	const ADRPlayerCharacter* Character,
	const FVector& RollDirection) const
{
	if (!IsValid(Character) || RollDirection.IsNearlyZero())
	{
		return NAME_None;
	}

	const float ForwardDot = FVector::DotProduct(Character->GetActorForwardVector(), RollDirection);
	const float RightDot = FVector::DotProduct(Character->GetActorRightVector(), RollDirection);
	const float DirectionAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));

	if (DirectionAngleDegrees >= -22.5f && DirectionAngleDegrees < 22.5f)
	{
		return FrontRollSection;
	}

	if (DirectionAngleDegrees >= 22.5f && DirectionAngleDegrees < 67.5f)
	{
		return FrontRightRollSection;
	}

	if (DirectionAngleDegrees >= 67.5f && DirectionAngleDegrees < 112.5f)
	{
		return RightRollSection;
	}

	if (DirectionAngleDegrees >= 112.5f && DirectionAngleDegrees < 157.5f)
	{
		return BackRightRollSection;
	}

	if (DirectionAngleDegrees >= 157.5f || DirectionAngleDegrees < -157.5f)
	{
		return BackRollSection;
	}

	if (DirectionAngleDegrees >= -157.5f && DirectionAngleDegrees < -112.5f)
	{
		return BackLeftRollSection;
	}

	if (DirectionAngleDegrees >= -112.5f && DirectionAngleDegrees < -67.5f)
	{
		return LeftRollSection;
	}

	return FrontLeftRollSection;
}

void UDRGA_CombatRollSkill::HandleRollFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UDRGA_CombatRollSkill::HandleRollInterrupted()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}
