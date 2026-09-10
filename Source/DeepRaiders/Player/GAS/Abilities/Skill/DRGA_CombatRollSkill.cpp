#include "DRGA_CombatRollSkill.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"

UDRGA_CombatRollSkill::UDRGA_CombatRollSkill()
{
	FGameplayTagContainer CombatRollAbilityTags;
	CombatRollAbilityTags.AddTag(DRGameplayTags::Ability_Action);
	CombatRollAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	CombatRollAbilityTags.AddTag(DRGameplayTags::Ability_Skill_CombatRoll);
	SetAssetTags(CombatRollAbilityTags);

	// 발사 입력을 누르고 있는 중에도 구르기는 발동해야 한다.
	// 구르기 시작 시 유지 중인 총격 Ability를 종료한다.
	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Attack_Ranged);
}

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

	StartRollGameplayCue(Character, RollDirection);

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRollFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRollInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRollInterrupted);

	// 다른 Ability가 몽타주 상태를 바꾸는 과정에서 종료 Delegate가 누락돼도
	// 구르기 GA가 계속 활성 상태로 남지 않도록, 재생 길이 기준 종료를 보장한다.
	UAbilityTask_WaitDelay* EndSafetyTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		RollDuration);
	if (IsValid(EndSafetyTask))
	{
		EndSafetyTask->OnFinish.AddDynamic(this, &ThisClass::HandleRollFinished);
		EndSafetyTask->ReadyForActivation();
	}

	MoveTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

void UDRGA_CombatRollSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool IsReplicateEndAbility,
	bool IsWasCancelled)
{
	StopRollGameplayCue();
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		IsReplicateEndAbility,
		IsWasCancelled);
}

FVector UDRGA_CombatRollSkill::ResolveRollDirection(const ADRPlayerCharacter* Character) const
{
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const FVector InputDirection = Character->GetSkillMovementDirection();
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

void UDRGA_CombatRollSkill::StartRollGameplayCue(
	ADRPlayerCharacter* Character,
	const FVector& RollDirection)
{
	if (IsRollGameplayCueActive)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(Character)
		|| !IsValid(AbilitySystem)
		|| RollDirection.IsNearlyZero())
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Character->GetActorLocation();
	Parameters.Normal = RollDirection.GetSafeNormal2D();
	Parameters.Instigator = Character;
	Parameters.EffectCauser = Character;
	Parameters.SourceObject = GetCurrentSkillDefinition();

	AbilitySystem->AddGameplayCue(
		DRGameplayTags::GameplayCue_VFX_Skill_CombatRoll,
		Parameters);
	IsRollGameplayCueActive = true;
}

void UDRGA_CombatRollSkill::StopRollGameplayCue()
{
	if (!IsRollGameplayCueActive)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->RemoveGameplayCue(
			DRGameplayTags::GameplayCue_VFX_Skill_CombatRoll);
	}

	IsRollGameplayCueActive = false;
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
