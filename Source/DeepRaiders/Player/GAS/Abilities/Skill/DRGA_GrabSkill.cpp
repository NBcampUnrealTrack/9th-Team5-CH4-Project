#include "DRGA_GrabSkill.h"

#include "AbilitySystemComponent.h"
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
	const bool IsLaunchValid = ResolveProjectileLaunch(
		ActorInfo,
		MaxDistance,
		SpawnLocation,
		ProjectileDirection);

	if (!IsValid(Character)
		|| !IsValid(AbilitySystem)
		|| !ProjectileClass
		|| MaxDistance <= 0.f
		|| PullSpeed <= 0.f
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
				MaxDistance,
				PullSpeed,
				PullDestinationDistance);

			UGameplayStatics::FinishSpawningActor(
				Projectile,
				SpawnTransform);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
