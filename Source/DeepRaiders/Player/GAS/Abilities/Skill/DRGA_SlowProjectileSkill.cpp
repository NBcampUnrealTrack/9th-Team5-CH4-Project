#include "DRGA_SlowProjectileSkill.h"

#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"

#include "DeepRaiders/Combat/Projectile/DRSlowProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

void UDRGA_SlowProjectileSkill::ActivateAbility(
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
		|| !SlowEffectClass
		|| EffectRadius <= 0.f
		|| ProjectileDirection.IsNearlyZero()
		|| !IsSpawnLocationValid
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		FGameplayEffectSpecHandle SlowEffectSpec =
			MakeOutgoingGameplayEffectSpec(SlowEffectClass, GetAbilityLevel());
		UWorld* World = Character->GetWorld();

		if (SlowEffectSpec.IsValid()
			&& IsValid(World))
		{
			const FTransform SpawnTransform(
				ProjectileDirection.Rotation(),
				SpawnLocation);
			ADRSlowProjectile* Projectile =
				World->SpawnActorDeferred<ADRSlowProjectile>(
					ProjectileClass,
					SpawnTransform,
					Character,
					Character,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (IsValid(Projectile))
			{
				Projectile->InitializeSlowProjectile(
					AbilitySystem,
					SlowEffectSpec,
					DRCombatTeam::GetActorTeamId(Character),
					EffectRadius);

				UGameplayStatics::FinishSpawningActor(
					Projectile,
					SpawnTransform);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
