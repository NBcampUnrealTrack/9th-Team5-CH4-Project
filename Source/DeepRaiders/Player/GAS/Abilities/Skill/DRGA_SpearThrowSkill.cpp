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
	FVector SpawnLocation;
	FVector ProjectileDirection;
	const bool IsLaunchValid = ResolveProjectileLaunch(
		ActorInfo,
		MaxAimDistance,
		SpawnLocation,
		ProjectileDirection);

	if (!IsValid(Character)
		|| !IsValid(AbilitySystem)
		|| !ProjectileClass
		|| KnockbackStrength < 0.f
		|| !IsLaunchValid
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		SpawnProjectile(
			Character,
			AbilitySystem,
			SpawnLocation,
			ProjectileDirection);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UDRGA_SpearThrowSkill::SpawnProjectile(
	ADRPlayerCharacter* Character,
	UAbilitySystemComponent* AbilitySystem,
	const FVector& SpawnLocation,
	const FVector& ProjectileDirection) const
{
	UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	if (!IsValid(World) || !IsValid(AbilitySystem) || !ProjectileClass)
	{
		return false;
	}

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

	if (!IsValid(Projectile))
	{
		return false;
	}

	Projectile->InitializeSpearProjectile(
		AbilitySystem,
		ImpactEffectSpecs,
		DRCombatTeam::GetActorTeamId(Character),
		KnockbackStrength);

	UGameplayStatics::FinishSpawningActor(
		Projectile,
		SpawnTransform);

	return true;
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
