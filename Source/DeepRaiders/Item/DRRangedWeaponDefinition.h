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

	FDRProjectileWeaponSnowAbsorbSettings()
		: bUseCameraAimCorrection(true)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	bool bEnabled = true;

	/** 흡수 프러스텀을 크로스헤어의 카메라 Trace 충돌 지점 방향으로 보정할지 여부. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	uint8 bUseCameraAimCorrection : 1;

	/** 카메라 충돌 지점 방향으로 허용할 최대 흡수 방향 보정 각도. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0", Units = "deg"))
	float MaxCameraAimCorrectionAngleDegrees = 30.0f;

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

	/** 캐릭터 로컬 좌표 기준 흡수 프러스텀 시작 오프셋. X: 전방, Y: 오른쪽, Z: 위. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb", meta = (Units = "cm"))
	FVector StartOffset = FVector::ZeroVector;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Snow", meta = (DisplayName = "Absorb"))
	FDRProjectileWeaponSnowAbsorbSettings SnowAbsorbSettings;
};
