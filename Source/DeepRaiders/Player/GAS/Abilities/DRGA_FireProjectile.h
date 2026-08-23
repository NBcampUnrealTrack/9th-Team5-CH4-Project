// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DRGA_RangedWeaponAttack.h"
#include "DRGA_FireProjectile.generated.h"

class ADRProjectile;

// DRProjectileWeaponItemDefinition 아이템의 Projectile 발사 GA
UCLASS()
class DEEPRAIDERS_API UDRGA_FireProjectile : public UDRGA_RangedWeaponAttack
{
	GENERATED_BODY()
	
protected:
	virtual bool IsAttackConfigurationValid() const override;
	virtual void OnRangedWeaponActivated() override;
	virtual void OnRangedWeaponEnded() override;
	virtual bool SendLocalShotRequest() override;	

private:
	void RegisterServerShotDelegate();
	void UnregisterServerShotDelegate();
	
	void HandleServerShotRequest();
	bool ExecuteServerProjectileShot();	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Projectile", meta = (AllowPrivateAccess))
	TSubclassOf<ADRProjectile> ProjectileClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Projectile", meta = (AllowPrivateAccess))
	FDRProjectileWorldImpactData WorldImpactData;
	
	FDelegateHandle ServerShotDelegateHandle;
};
