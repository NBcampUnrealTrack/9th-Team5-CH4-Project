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
struct DEEPRAIDERS_API FDRRangedWeaponHeatSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Heat")
	bool bEnabled = true;

	/** Projectile/HitScan처럼 발 단위 무기가 서버에서 1회 성공 발사할 때 추가되는 Heat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Heat", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HeatPerShot = 10.f;

	/** Sprayer 같은 지속형 무기가 초당 추가할 Heat. 실제 적용은 SprayTickInterval을 곱한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Heat", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HeatPerSecond = 20.f;

	/** 마지막 Heat 증가 이후 자연 냉각이 시작되기까지의 대기 시간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Heat", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DecayDelay = 1.f;

	/** MaxHeat에서 0까지 자연 냉각되는 데 걸리는 시간. 현재 Heat가 낮으면 비례해서 더 빨리 0에 도달한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Heat", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float RecoveryDuration = 4.f;
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

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRRangedWeaponDataTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHeatEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeatPerShot = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeatPerSecond = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeatDecayDelay = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeatRecoveryDuration = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSnowAbsorbEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowAbsorbRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowAbsorbPower = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowAbsorbSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowAbsorbRange = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowAbsorbSweepRadius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SnowAbsorbMaxSweepsPerTick = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSnowAbsorbUseAdaptiveQuery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowAbsorbInnerRadiusRatio = 0.5f;
	
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Heat", meta = (DisplayName = "Heat"))
	FDRRangedWeaponHeatSettings HeatSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Absorb"))
	FDRProjectileWeaponSnowAbsorbSettings SnowAbsorbSettings;
};
