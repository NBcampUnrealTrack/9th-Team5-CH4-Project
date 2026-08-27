// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRItemDefinition.h"
#include "DRProjectileWeaponDefinition.generated.h"

class UGameplayEffect;

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

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRProjectileWeaponSnowAbsorbSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	bool bEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float Radius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Power = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Speed = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float Range = 300.f;

	// 무기 중심에서 조준 방향으로 시작점을 이동한다. 음수면 무기 중심 뒤에서 시작한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (Units = "cm"))
	float StartOffset = -50.f;

	// frustum 시작 원의 반경 비율이다. 1이면 끝 원과 같은 크기다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float InnerRadiusRatio = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	EDRSnowRemovalBrushShape BrushShape = EDRSnowRemovalBrushShape::Sphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	EDRSnowRemovalMode RemovalMode = EDRSnowRemovalMode::AbsorbTool;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRProjectileWeaponItemDefinition : public UDRItemDefinition
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Add"))
	FDRProjectileWeaponSnowAddSettings SnowAddSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Absorb"))
	FDRProjectileWeaponSnowAbsorbSettings SnowAbsorbSettings;
};
