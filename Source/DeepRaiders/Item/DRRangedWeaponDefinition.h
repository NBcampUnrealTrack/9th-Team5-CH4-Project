#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRRangedWeaponDefinition.generated.h"

/*
 * 원거리 무기 공통 Definition.
 *
 * Projectile / HitScan / Sprayer 등
 * 원거리 무기 계열에서 공통으로 사용하는 데이터를 가진다.
 */

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float SweepRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSweepsPerTick = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (DisplayName = "Use Adaptive Query"))
	bool bUseAdaptiveQuery = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (Units = "cm"))
	float StartOffset = -50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float InnerRadiusRatio = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	EDRSnowRemovalBrushShape BrushShape = EDRSnowRemovalBrushShape::Sphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	EDRSnowRemovalMode RemovalMode = EDRSnowRemovalMode::AbsorbTool;
};

UCLASS(Abstract, BlueprintType)
class DEEPRAIDERS_API UDRRangedWeaponDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRRangedWeaponDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Absorb"))
	FDRProjectileWeaponSnowAbsorbSettings SnowAbsorbSettings;
};
