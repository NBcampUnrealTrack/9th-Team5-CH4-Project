// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DRProjectileWeaponDefinition.generated.h"

class ADRProjectile;
class UGameplayEffect;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRProjectileWeaponItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()
	
public:
	// 발사할 Projectile Class
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	TSubclassOf<ADRProjectile> ProjectileClass = nullptr;
	
	// 플레이어 적중 시 사용할 Effect 리스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	TArray<FDRProjectileImpactEffect> ImpactEffects;
	
	// 월드 적중 시 사용할 눈 생성 데이터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	FDRProjectileWorldImpactData WorldImpactData;
	
	// ProjectileSpawn 전방 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = 0.0, UIMin=0.0))
	float SpawnForwardOffset = 100.0f;
	
	// ProjectileSpawn 높이 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = 0.0, UIMin=0.0))
	float SpawnHeightOffset = 60.0f;
	
	// 다음 발사까지의 발사 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire", meta = (ClampMin = 0.01, UIMin=0.01, Units = "s"))
	float BaseFireInterval = 0.25f;
	
	// 1회 발사 시 소비할 SnowGauge
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SnowCostPerShot = 1.f;

	// SnowGauge 소비용 GameplayEffect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	TSubclassOf<UGameplayEffect> SnowCostEffectClass;
};
