// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGA_FireProjectile.h"

#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRGA_FireProjectile::UDRGA_FireProjectile()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;	
}

void UDRGA_FireProjectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	UDRProjectileWeaponItemDefinition* WeaponDefinition = 
		Cast<UDRProjectileWeaponItemDefinition>(GetSourceObject(Handle, ActorInfo));
	
	if (!IsValid(WeaponDefinition)
		|| !WeaponDefinition->ProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// LocalPredicted GA는 클라이언트와 서버 양쪽에서 실행된다.
	// 실제 Projectile 생성은 서버에서만 수행
	if (ActorInfo->IsNetAuthority())
	{
		UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
		
		TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
		BuildImpactEffectSpecs(AbilitySystemComponent, WeaponDefinition, ImpactEffectSpecs);
		SpawnProjectile(ActorInfo, WeaponDefinition, ImpactEffectSpecs);
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);	
}

void UDRGA_FireProjectile::BuildImpactEffectSpecs(UAbilitySystemComponent* AbilitySystemComponent,
	UDRProjectileWeaponItemDefinition* WeaponDefinition, TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();
	
	if (!IsValid(AbilitySystemComponent)
		|| !IsValid(WeaponDefinition))
	{
		return;
	}
	
	for (const FDRProjectileImpactEffect& EffectData : WeaponDefinition->ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}
		
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(WeaponDefinition);
		
		FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(
			EffectData.EffectClass,
			EffectData.EffectLevel,
			EffectContext);
		
		if (!EffectSpec.IsValid())
		{
			continue;
		}
		
		for (const TPair<FGameplayTag, float>& Pair : EffectData.SetByCallerMagnitudes)
		{
			if (Pair.Key.IsValid())
			{
				EffectSpec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
			}
		}
		
		OutEffectSpecs.Add(EffectSpec);
	}
}

bool UDRGA_FireProjectile::SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo,
	UDRProjectileWeaponItemDefinition* WeaponDefinition,
	const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const
{
	if (ActorInfo == nullptr 
		|| !IsValid(WeaponDefinition))
	{
		return false;
	}
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
	
	if (!IsValid(AvatarActor)
		|| !IsValid(AbilitySystemComponent))
	{
		return false;
	}
	
	UWorld* World = AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		return false;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;
	
	if (AController* Controller = ActorInfo->PlayerController.Get())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{	
		AvatarActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}
	
	const FVector SpawnLocation =AvatarActor->GetActorLocation() + FVector::UpVector * WeaponDefinition->SpawnHeightOffset 
		+ ViewRotation.Vector() * WeaponDefinition->SpawnForwardOffset;
	
	const FTransform SpawnTransform(ViewRotation, SpawnLocation);
	
	APawn* InstigatorPawn = Cast<APawn>(AvatarActor);
	
	ADRProjectile* Projectile = World->SpawnActorDeferred<ADRProjectile>(WeaponDefinition->ProjectileClass, 
		SpawnTransform, AvatarActor, InstigatorPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	
	if (!IsValid(Projectile))
	{
		return false;
	}
	
	int32 SourceTeamId = INDEX_NONE;
	
	if (const ADRPlayerState* DRPlayerState = Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()))
	{
		SourceTeamId = DRPlayerState->GetTeamId();
	}
	
	Projectile->InitializeProjectile(AbilitySystemComponent, ImpactEffectSpecs, WeaponDefinition->WorldImpactData, SourceTeamId);
	
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	
	return true;	
}
