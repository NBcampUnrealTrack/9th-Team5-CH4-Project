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
struct DEEPRAIDERS_API FDRRangedWeaponAimCorrectionSettings
{
	GENERATED_BODY()

	FDRRangedWeaponAimCorrectionSettings()
		: bUseCameraAimCorrection(true)
	{
	}

	/** 카메라 Trace 충돌 지점 방향으로 원거리 무기의 조준 방향을 보정할지 여부. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim Correction")
	uint8 bUseCameraAimCorrection : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim Correction", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MinCameraAimCorrectionDistance = 100.0f;

	/** 카메라 충돌 지점 방향으로 허용할 최대 조준 방향 보정 각도. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim Correction", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0", Units = "deg"))
	float MaxCameraAimCorrectionAngleDegrees = 30.0f;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float SweepRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSweepsPerTick = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (DisplayName = "Use Adaptive Query"))
	bool bUseAdaptiveQuery = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float InnerRadiusRatio = 0.5f;

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

	FVector ResolveCameraAimDirection(
		const FVector& ViewDirection,
		const FVector& Origin,
		const FVector& CameraAimPoint) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Aim")
	FDRRangedWeaponAimCorrectionSettings AimCorrectionSettings;

	/** 캐릭터 로컬 좌표 기준 흡수 및 투사체 시작 오프셋. X: 전방, Y: 오른쪽, Z: 위. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Origin", meta = (DisplayName = "Start Offset", Units = "cm"))
	FVector StartOffset = FVector(-15.0f, 10.0f, 10.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Absorb"))
	FDRProjectileWeaponSnowAbsorbSettings SnowAbsorbSettings;
};
