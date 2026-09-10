#include "DRGA_SuperJumpSkill.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

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
	UDRCharacterMovementComponent* MovementComponent = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	if (!IsValid(Character)
		|| !IsValid(MovementComponent)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ClearPendingLandingEffects();
	MovementComponent->ActivateSuperJumpAirControl(
		AirControl,
		MaxAirSpeedMultiplier);

	TArray<FGameplayEffectSpecHandle> LandingEffectSpecs;
	if (ActorInfo->IsNetAuthority())
	{
		const ADRPlayerState* PlayerState =
			Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get());
		const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
			? PlayerState->GetPerkComponent()
			: nullptr;
		const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
		if (IsValid(PerkComponent) && IsValid(SkillDefinition))
		{
			PerkComponent->BuildEquippedSkillEffectSpecs(
				ActorInfo->AbilitySystemComponent.Get(),
				SkillDefinition->SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				LandingEffectSpecs);
		}
	}

	if (!LandingEffectSpecs.IsEmpty())
	{
		PendingLandingEffectSpecs = MoveTemp(LandingEffectSpecs);
		LandingSourceAbilitySystem = ActorInfo->AbilitySystemComponent.Get();
		LandingSourceCharacter = Character;
		LandingMovementComponent = MovementComponent;
		PendingSuperJumpSequence = MovementComponent->GetSuperJumpSequence();
		MovementComponent->OnCharacterLanded.AddUObject(
			this, &ThisClass::HandleCharacterLanded);
	}

	const float GravityMagnitude = FMath::Abs(MovementComponent->GetGravityZ());
	const float JumpVelocity = GravityMagnitude > KINDA_SMALL_NUMBER
		? FMath::Sqrt(2.0f * GravityMagnitude * FMath::Max(JumpHeight, 0.0f))
		: 0.0f;
	Character->LaunchCharacter(FVector::UpVector * JumpVelocity, false, true);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDRGA_SuperJumpSkill::HandleCharacterLanded(const FHitResult& /*Hit*/)
{
	UDRCharacterMovementComponent* MovementComponent = LandingMovementComponent.Get();
	if (!IsValid(MovementComponent)
		|| PendingSuperJumpSequence == 0
		|| MovementComponent->GetSuperJumpSequence() != PendingSuperJumpSequence)
	{
		ClearPendingLandingEffects();
		return;
	}

	ApplyLandingEffects();
	ClearPendingLandingEffects();
}

void UDRGA_SuperJumpSkill::ApplyLandingEffects()
{
	ADRPlayerCharacter* SourceCharacter = LandingSourceCharacter.Get();
	UAbilitySystemComponent* SourceAbilitySystem = LandingSourceAbilitySystem.Get();
	UWorld* World = IsValid(SourceCharacter) ? SourceCharacter->GetWorld() : nullptr;
	if (!IsValid(SourceCharacter)
		|| !SourceCharacter->HasAuthority()
		|| !IsValid(SourceAbilitySystem)
		|| !IsValid(World)
		|| LandingEffectRadius <= 0.f
		|| PendingLandingEffectSpecs.IsEmpty())
	{
		return;
	}

	const int32 SourceTeamId = DRCombatTeam::GetActorTeamId(SourceCharacter);
	if (SourceTeamId == INDEX_NONE)
	{
		return;
	}

	// 넉백 GE는 EffectContext Origin을 기준으로 대상의 밀려날 방향을 계산한다.
	// Spec은 착지 시점까지 보관되므로, SuperJump 발동 위치가 아니라 실제 착지 위치를 기록한다.
	const FVector LandingOrigin = SourceCharacter->GetActorLocation();
	FGameplayCueParameters CueParameters;
	CueParameters.Location = LandingOrigin;
	CueParameters.Normal = FVector::UpVector;
	CueParameters.Instigator = SourceCharacter;
	CueParameters.EffectCauser = SourceCharacter;
	SourceAbilitySystem->ExecuteGameplayCue(
		DRGameplayTags::GameplayCue_VFX_Skill_SuperJump_HeroLanding,
		CueParameters);
	for (const FGameplayEffectSpecHandle& EffectSpec : PendingLandingEffectSpecs)
	{
		if (EffectSpec.IsValid())
		{
			EffectSpec.Data->GetContext().AddOrigin(LandingOrigin);
		}
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SuperJumpLanding), false, SourceCharacter);
	const FCollisionObjectQueryParams ObjectQueryParams(ECC_Pawn);
	World->OverlapMultiByObjectType(
		OverlapResults,
		SourceCharacter->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(LandingEffectRadius),
		QueryParams);

	TSet<UAbilitySystemComponent*> AppliedTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!IsValid(TargetActor)
			|| DRCombatTeam::IsFriendlyTarget(SourceTeamId, TargetActor))
		{
			continue;
		}

		const IAbilitySystemInterface* AbilitySystemInterface =
			Cast<IAbilitySystemInterface>(TargetActor);
		UAbilitySystemComponent* TargetAbilitySystem = AbilitySystemInterface != nullptr
			? AbilitySystemInterface->GetAbilitySystemComponent()
			: nullptr;
		if (!IsValid(TargetAbilitySystem)
			|| AppliedTargets.Contains(TargetAbilitySystem))
		{
			continue;
		}

		AppliedTargets.Add(TargetAbilitySystem);
		for (const FGameplayEffectSpecHandle& EffectSpec : PendingLandingEffectSpecs)
		{
			if (EffectSpec.IsValid())
			{
				SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
					*EffectSpec.Data.Get(), TargetAbilitySystem);
			}
		}
	}
}

void UDRGA_SuperJumpSkill::ClearPendingLandingEffects()
{
	if (UDRCharacterMovementComponent* MovementComponent =
		LandingMovementComponent.Get())
	{
		MovementComponent->OnCharacterLanded.RemoveAll(this);
	}

	PendingLandingEffectSpecs.Reset();
	LandingSourceAbilitySystem.Reset();
	LandingSourceCharacter.Reset();
	LandingMovementComponent.Reset();
	PendingSuperJumpSequence = 0;
}
