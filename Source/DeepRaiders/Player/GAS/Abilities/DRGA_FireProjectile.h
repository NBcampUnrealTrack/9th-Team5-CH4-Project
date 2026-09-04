// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRGA_RangedWeaponAttack.h"
#include "DRGA_FireProjectile.generated.h"

class ADRProjectile;

// DRProjectileWeaponItemDefinition 아이템의 Projectile 발사 GA
UCLASS()
class DEEPRAIDERS_API UDRGA_FireProjectile : public UDRGA_RangedWeaponAttack
{
	GENERATED_BODY()
	
protected:
	virtual bool IsAttackConfigurationValid(const UDRProjectileWeaponItemDefinition* WeaponDefinition) const override;
	virtual void OnRangedWeaponActivated() override;
	virtual void OnRangedWeaponEnded() override;
	virtual bool SendLocalShotRequest() override;	

private:
	void RegisterServerShotTargetDataDelegate();
	void UnregisterServerShotTargetDataDelegate();

	void HandleServerShotTargetData(
		const FGameplayAbilityTargetDataHandle& TargetData,
		FGameplayTag ApplicationTag);

	bool ValidateServerShotTargetData(
		const FGameplayAbilityTargetDataHandle& TargetData,
		FVector& OutAimPoint,
		FVector& OutAimDirection) const;

	bool ExecuteServerProjectileShot(
		const FVector& AimPoint,
		const FVector& AimDirection);

	bool ResolveProjectileLaunchVelocity(
		const FVector& SpawnLocation,
		const FVector& AimPoint,
		FVector& OutLaunchVelocity) const;

	bool SpawnProjectile(
		const FVector& SpawnLocation,
		const FVector& LaunchVelocity,
		AActor* AvatarActor,
		UAbilitySystemComponent* AbilitySystem,
		const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs);
	
	FDelegateHandle ServerShotTargetDataDelegateHandle;
};
