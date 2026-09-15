#include "DRGA_GrabSkill.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Effects/DRGE_GrabDebuff.h"
#include "Kismet/GameplayStatics.h"

#include "DeepRaiders/Combat/Projectile/DRGrabProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

UDRGA_GrabSkill::UDRGA_GrabSkill()
{
	ProjectileClass = ADRGrabProjectile::StaticClass();
}

void UDRGA_GrabSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	UAbilitySystemComponent* AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	FVector SpawnLocation;
	FVector ProjectileDirection;
	const float EffectiveMaxDistance = MaxDistance + FMath::Max(0.f, GetPerkValue(
		DRGameplayTags::Perk_Skill_Grab_Enhancement, DRGameplayTags::Data_Perk_Grab_RangeBonus));
	const bool IsLaunchValid = ResolveProjectileLaunch(
		ActorInfo,
		EffectiveMaxDistance,
		SpawnLocation,
		ProjectileDirection);

	if (!IsValid(Character)
		|| !IsValid(AbilitySystem)
		|| !ProjectileClass
		|| MaxDistance <= 0.f
		|| PullSpeed <= 0.f
		|| MaxPullDuration <= 0.f
		|| PullDestinationDistance < 0.f
		|| !IsLaunchValid
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		UWorld* World = Character->GetWorld();
		const FTransform SpawnTransform(
			ProjectileDirection.Rotation(),
			SpawnLocation);
		ADRGrabProjectile* Projectile =
			IsValid(World)
			? World->SpawnActorDeferred<ADRGrabProjectile>(
				ProjectileClass,
				SpawnTransform,
				Character,
				Character,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;

		if (IsValid(Projectile))
		{
			Projectile->InitializeGrabProjectile(
				AbilitySystem,
				DRCombatTeam::GetActorTeamId(Character),
				EffectiveMaxDistance,
				PullSpeed,
				PullDestinationDistance,
				BuildImpactEffectSpecs(),
				1.f + FMath::Max(0.f, GetPerkValue(
					DRGameplayTags::Perk_Skill_Grab_Enhancement,
					DRGameplayTags::Data_Perk_Grab_HitScaleBonus)),
				BuildArrivalSlowSpec(),
				MaxPullDuration);

			UGameplayStatics::FinishSpawningActor(
				Projectile,
				SpawnTransform);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

float UDRGA_GrabSkill::GetPerkValue(FGameplayTag PerkTag, FGameplayTag ValueTag) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent() : nullptr;
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	return IsValid(PerkComponent) && IsValid(SkillDefinition)
		? PerkComponent->GetSkillPerkEffectValue(
			SkillDefinition->SkillId, PerkTag, EDRSkillEffectTrigger::OnSkillCommitted, ValueTag)
		: 0.f;
}

TArray<FGameplayEffectSpecHandle> UDRGA_GrabSkill::BuildImpactEffectSpecs() const
{
	TArray<FGameplayEffectSpecHandle> Specs;
	const float SnowReduction = GetPerkValue(
		DRGameplayTags::Perk_Skill_Grab_Debuff, DRGameplayTags::Data_Perk_Grab_SnowReduction);
	if (SnowReduction > 0.f && SnowCostEffectClass)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
			SnowCostEffectClass, GetAbilityLevel());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, -SnowReduction);
			Specs.Add(Spec);
		}
	}
	return Specs;
}

FGameplayEffectSpecHandle UDRGA_GrabSkill::BuildArrivalSlowSpec() const
{
	const float SlowDuration = GetPerkValue(
		DRGameplayTags::Perk_Skill_Grab_Debuff, DRGameplayTags::Data_Effect_Duration);
	FGameplayEffectSpecHandle Spec;
	if (SlowDuration > 0.f)
	{
		Spec = MakeOutgoingGameplayEffectSpec(
			UDRGE_GrabSlow::StaticClass(), GetAbilityLevel());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Effect_Duration, SlowDuration);
		}
	}
	return Spec;
}
