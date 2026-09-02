// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRRangedWeaponDefinition.h"
#include "DRWeaponPresentationTypes.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DRProjectileWeaponDefinition.generated.h"

class UGameplayEffect;
class ADRProjectile;

UENUM(BlueprintType)
enum class EDRProjectileWeaponResourceType : uint8
{
	SnowGauge UMETA(DisplayName = "Snow Gauge"),
	InstanceAmmo UMETA(DisplayName = "Instance Ammo")
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRProjectileWeaponSnowAddSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Add")
	bool bEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Add", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float Radius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Add", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Amount = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Add")
	EDRSnowVoxelEditTool EditTool = EDRSnowVoxelEditTool::DirectionalSurfaceTool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Add")
	bool bAllowVirtualSurfaceFallback = true;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRProjectileWeaponItemDefinition : public UDRRangedWeaponDefinition
{
	GENERATED_BODY()
	
public:
	UDRProjectileWeaponItemDefinition();
	
	// 발사할 때 소비할 자원의 종류
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource")
	EDRProjectileWeaponResourceType ResourceType = EDRProjectileWeaponResourceType::SnowGauge;	

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

	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly,Category = "Ranged Weapon|Fire",meta = (
	ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float BaseFireInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Fire")
	bool bAutomaticFire = false;

	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly,Category = "Ranged Weapon|Aim",meta = (
		ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float MaxAttackDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Effect")
	TArray<FDRGameplayEffectData> ImpactEffects;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Effect", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BreakableDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Projectile", meta = (AllowPrivateAccess))
	TSubclassOf<ADRProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Projectile", meta = (AllowPrivateAccess,
		ClampMin = "1", UIMin = "1"))
	int32 ProjectileCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Projectile", meta = (AllowPrivateAccess,
		ClampMin = "0.0", UIMin = "0.0", Units = "deg"))
	float SpreadHalfAngleDegrees = 0.f;
	
	// 관통 가능 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|HitScan", meta = (AllowPrivateAccess))
	bool bCanPenetrateTargets = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|HitScan|Validation",
		meta = (AllowPrivateAccess, ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float MaxServerAimDeviationDegrees = 30.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Add"))
	FDRProjectileWeaponSnowAddSettings SnowAddSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation")
	FDRWeaponPresentationData FirePresentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation")
	FDRWeaponPresentationData ImpactPresentation;
};
