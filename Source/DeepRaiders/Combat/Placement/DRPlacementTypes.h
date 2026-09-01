#pragma once

#include "CoreMinimal.h"
#include "DRPlacementTypes.generated.h"

/** 설치형 스킬이 공통으로 사용하는 조준 및 설치 가능 여부 판정 설정이다. */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRPlacementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Targeting")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	/** 바닥으로 인정할 최대 경사. 벽/천장은 대상이 될 수 없도록 45도를 상한으로 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float MaxFloorSlopeDegrees = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", Units = "cm"))
	float ServerViewOriginTolerance = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float ServerAimAngleTolerance = 5.f;
};
