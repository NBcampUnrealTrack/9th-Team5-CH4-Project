#pragma once

#include "CoreMinimal.h"
#include "DRMiningTypes.generated.h"

USTRUCT(BlueprintType)
struct FDRMiningSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining", meta = (ClampMin = "0.0", Units = "cm"))
	float MineTraceDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining", meta = (ClampMin = "0.0", Units = "cm"))
	float MineRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MineSurfaceDepthRatio = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining", meta = (ClampMin = "0.0", Units = "s"))
	float MineCooldown = 0.25f;
};
