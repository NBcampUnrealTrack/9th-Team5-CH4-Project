#include "DRGA_SpearThrowSkill.h"

#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"

#include "DeepRaiders/Combat/Projectile/DRSpearProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

void UDRGA_SpearThrowSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	UAbilitySystemComponent* AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FVector ProjectileDirection =
		IsValid(Character) ? Character->GetBaseAimRotation().Vector().GetSafeNormal() : FVector::ZeroVector;

	FVector SpawnLocation;
	const bool IsSpawnLocationValid =
		IsValid(Character)
		&& Character->CalculateGameplayFireOrigin(ProjectileDirection, SpawnLocation);

	if (!IsValid(Character)
		|| !IsValid(AbilitySystem)
		|| !ProjectileClass
		|| KnockbackStrength < 0.f
		|| ProjectileDirection.IsNearlyZero()
		|| !IsSpawnLocationValid
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		UWorld* World = Character->GetWorld();
		if (IsValid(World))
		{
			TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
			BuildImpactEffectSpecs(AbilitySystem, ImpactEffectSpecs);

			const FTransform SpawnTransform(
				ProjectileDirection.Rotation(),
				SpawnLocation);
			ADRSpearProjectile* Projectile =
				World->SpawnActorDeferred<ADRSpearProjectile>(
					ProjectileClass,
					SpawnTransform,
					Character,
					Character,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (IsValid(Projectile))
			{
				Projectile->InitializeSpearProjectile(
					AbilitySystem,
					ImpactEffectSpecs,
					DRCombatTeam::GetActorTeamId(Character),
					KnockbackStrength);

				UGameplayStatics::FinishSpawningActor(
					Projectile,
					SpawnTransform);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDRGA_SpearThrowSkill::BuildImpactEffectSpecs(
	UAbilitySystemComponent* AbilitySystem,
	TArray<FGameplayEffectSpecHandle>& OutImpactEffectSpecs) const
{
	OutImpactEffectSpecs.Reset();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	for (const FDRGameplayEffectData& EffectData : ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
		FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
			EffectData.EffectClass,
			EffectData.EffectLevel,
			EffectContext);

		if (!EffectSpec.IsValid())
		{
			continue;
		}

		for (const TPair<FGameplayTag, float>& Magnitude : EffectData.SetByCallerMagnitudes)
		{
			if (Magnitude.Key.IsValid())
			{
				EffectSpec.Data->SetSetByCallerMagnitude(
					Magnitude.Key,
					Magnitude.Value);
			}
		}

		OutImpactEffectSpecs.Add(EffectSpec);
	}
}
