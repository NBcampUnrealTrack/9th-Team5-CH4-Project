// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DRProjectileWeaponDefinition.generated.h"

class ADRProjectile;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EDRProjectileWeaponResourceType : uint8
{
	SnowGauge UMETA(DisplayName = "Snow Gauge"),
	InstanceAmmo UMETA(DisplayName = "Instance Ammo")
};

UENUM(BlueprintType)
enum class EDRRangedWeaponAttackType :uint8
{
	Projectile UMETA(DisplayName = "Projectile"),
	HitScan UMETA(DisplayName = "HitScan"),
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRProjectileWeaponItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()
	
public:
	UDRProjectileWeaponItemDefinition();
	
	// 발사할 Projectile Class
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (
		EditCondition = "AttackType == EDRRangedWeaponAttackType::Projectile"))
	TSubclassOf<ADRProjectile> ProjectileClass = nullptr;
	
	// 플레이어 적중 시 사용할 Effect 리스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (
		EditCondition = "AttackType == EDRRangedWeaponAttackType::Projectile"))
	TArray<FDRProjectileImpactEffect> ImpactEffects;
	
	// 월드 적중 시 사용할 눈 생성 데이터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (
		EditCondition = "AttackType == EDRRangedWeaponAttackType::Projectile"))
	FDRProjectileWorldImpactData WorldImpactData;
	
	// ProjectileSpawn 전방 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (
		EditCondition = "AttackType == EDRRangedWeaponAttackType::Projectile", ClampMin = 0.0, UIMin=0.0))
	float SpawnForwardOffset = 100.0f;
	
	// ProjectileSpawn 높이 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (
		EditCondition = "AttackType == EDRRangedWeaponAttackType::Projectile", ClampMin = 0.0, UIMin=0.0))
	float SpawnHeightOffset = 60.0f;
	
	// 다음 발사까지의 발사 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire", meta = (ClampMin = 0.01, UIMin=0.01, Units = "s"))
	float BaseFireInterval = 0.25f;
	
	// 발사할 때 소비할 자원의 종류
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource")
	EDRProjectileWeaponResourceType ResourceType = EDRProjectileWeaponResourceType::SnowGauge;	
	
	// 원거리 무기 공격 방식
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EDRRangedWeaponAttackType AttackType = EDRRangedWeaponAttackType::Projectile;
	
	// ResourceType::SnowGauge
	// 1회 발사 시 소비할 SnowGauge
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Snow", meta = (
		EditCondition = "ResourceType == EDRProjectileWeaponResourceType::SnowGauge", ClampMin = "0.0", UIMin = "0.0"))
	float SnowCostPerShot = 1.f;

	// SnowGauge 소비용 GameplayEffect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Snow", meta = (
		EditCondition = "ResourceType == EDRProjectileWeaponResourceType::SnowGauge"))
	TSubclassOf<UGameplayEffect> SnowCostEffectClass;
	
	// ResourceType::Ammo
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Ammo", meta = (
	EditCondition = "ResourceType == EDRProjectileWeaponResourceType::InstanceAmmo", ClampMin = "1", UIMin = "1"))
	int32 InitialAmmo = 1;
};
