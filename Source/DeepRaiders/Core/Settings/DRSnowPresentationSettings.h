#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DRSnowPresentationSettings.generated.h"

class UMaterialParameterCollection;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Snow Presentation"))
class DEEPRAIDERS_API UDRSnowPresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	UPROPERTY(Config, EditAnywhere, Category = "WPO")
	TSoftObjectPtr<UMaterialParameterCollection> WPOParameterCollection;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float StartDelay = 0.08f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float MinimumDuration = 0.6;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float MaximumDuration = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Scale", meta = (ClampMin = "1.0", Units = "cm"))
	float FullStrengthRadius = 300.f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Scale", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumStrength = 0.2f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Shape", meta = (ClampMin = "0.0"))
	float CollapseHeightRatio = 0.3f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Shape", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumCollapseHeight = 80.f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Shape", meta = (ClampMin = "0.0"))
	float EdgeWidthRatio = 0.18f;

	UPROPERTY(Config, EditAnywhere, Category = "WPO|Shape", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumEdgeWidth = 10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Previous Surface")
	bool bEnablePreviousSurfaceSnapshots = true;

	UPROPERTY(Config, EditAnywhere, Category = "Previous Surface", meta = (ClampMin = "1"))
	int32 MaximumSnapshotComponents = 32;

	UPROPERTY(Config, EditAnywhere, Category = "Previous Surface", meta = (ClampMin = "0.0", Units = "s"))
	float SnapshotLifetimePadding = 0.15f;
};
