// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRGA_RangedWeaponAttack.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DRGA_FireProjectile.generated.h"

class ADRProjectile;

USTRUCT()
struct FDRGameplayAbilityTargetData_ProjectileShot
	: public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	FDRGameplayAbilityTargetData_ProjectileShot() = default;

	FDRGameplayAbilityTargetData_ProjectileShot(
		const FHitResult& InHitResult,
		uint32 InShotSequence)
		: FGameplayAbilityTargetData_SingleTargetHit(
			InHitResult)
		, ShotSequence(InShotSequence)
	{
	}

	UPROPERTY()
	uint32 ShotSequence = 0;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	bool NetSerialize(
		FArchive& Ar,
		UPackageMap* Map,
		bool& bOutSuccess)
	{
		const bool bBaseSuccess =
			FGameplayAbilityTargetData_SingleTargetHit::
			NetSerialize(
				Ar,
				Map,
				bOutSuccess);

		Ar.SerializeIntPacked(ShotSequence);

		bOutSuccess =
			bOutSuccess
			&& bBaseSuccess
			&& !Ar.IsError();

		return bOutSuccess;
	}
};

template<>
struct TStructOpsTypeTraits<
	FDRGameplayAbilityTargetData_ProjectileShot>
	: public TStructOpsTypeTraitsBase2<
		FDRGameplayAbilityTargetData_ProjectileShot>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

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
		FVector& OutAimDirection,
		uint32& OutShotSequence) const;

	bool ExecuteServerProjectileShot(
		const FVector& AimPoint,
		const FVector& AimDirection,
		uint32 ShotSequence);

	bool ResolveProjectileLaunchVelocity(
		const FVector& SpawnLocation,
		const FVector& AimPoint,
		FVector& OutLaunchVelocity) const;

	/** Owning client 전용. 현재는 1발 + 무산포 무기만 local predicted visual을 생성한다. */
	void TrySpawnLocalVisualProjectile(
		const FVector& SpawnLocation,
		const FVector& AimPoint,
		uint32 ShotSequence);

	bool SpawnProjectile(
		const FVector& SpawnLocation,
		const FVector& LaunchVelocity,
		AActor* AvatarActor,
		UAbilitySystemComponent* AbilitySystem,
		const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs,
		uint32 ShotSequence);
	
	FDelegateHandle ServerShotTargetDataDelegateHandle;
	uint32 LocalShotSequence = 0;
};
