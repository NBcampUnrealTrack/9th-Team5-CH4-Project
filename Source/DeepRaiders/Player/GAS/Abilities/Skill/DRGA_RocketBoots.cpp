#include "DRGA_RocketBoots.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

UDRGA_RocketBoots::UDRGA_RocketBoots()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(DRGameplayTags::Ability_Action);
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
	return IsValid(MovementComponent)
		&& MovementComponent->IsFalling()
		&& MovementComponent->IsSuperJumpActive()
		&& MovementComponent->GetSuperJumpSequence() != ConsumedSuperJumpSequence
		&& ImpulseStrength > KINDA_SMALL_NUMBER;
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
	if (!IsValid(Character)
		|| !IsValid(MovementComponent)
		|| !MovementComponent->IsSuperJumpActive()
		|| MovementComponent->GetSuperJumpSequence() == ConsumedSuperJumpSequence
		|| ImpulseStrength <= KINDA_SMALL_NUMBER
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

	DashDirection = DashDirection.GetSafeNormal2D();
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 목표 위치를 고정하지 않고, 입력 순간의 방향으로 한 번만 추진력을 더한다.
	MovementComponent->ClearAirborneMomentumPreservation();
	MovementComponent->AddImpulse(DashDirection * ImpulseStrength, true);
	ConsumedSuperJumpSequence = MovementComponent->GetSuperJumpSequence();
	StartDashGameplayCue(Character, DashDirection);

	UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(
		this, DashGameplayCueDuration);
	if (!IsValid(EndTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	EndTask->OnFinish.AddDynamic(this, &ThisClass::HandleDashGameplayCueFinished);
	EndTask->ReadyForActivation();
}

void UDRGA_RocketBoots::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool IsReplicateEndAbility,
	bool IsWasCancelled)
{
	StopDashGameplayCue();
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		IsReplicateEndAbility,
		IsWasCancelled);
}

void UDRGA_RocketBoots::StartDashGameplayCue(
	ADRPlayerCharacter* Character,
	const FVector& DashDirection)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (IsDashGameplayCueActive
		|| !IsValid(Character)
		|| !IsValid(AbilitySystem)
		|| DashDirection.IsNearlyZero())
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Character->GetActorLocation();
	Parameters.Normal = DashDirection.GetSafeNormal2D();
	Parameters.Instigator = Character;
	Parameters.EffectCauser = Character;
	AbilitySystem->AddGameplayCue(
		DRGameplayTags::GameplayCue_VFX_Skill_ForwardDash,
		Parameters);
	IsDashGameplayCueActive = true;
}

void UDRGA_RocketBoots::StopDashGameplayCue()
{
	if (!IsDashGameplayCueActive)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->RemoveGameplayCue(
			DRGameplayTags::GameplayCue_VFX_Skill_ForwardDash);
	}

	IsDashGameplayCueActive = false;
}

void UDRGA_RocketBoots::HandleDashGameplayCueFinished()
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
