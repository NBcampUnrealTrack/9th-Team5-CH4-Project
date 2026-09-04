#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DRUpgradeTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRStatUpgradeData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	FGameplayTag UpgradeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UTexture2D> Icon;

	/** 모든 단계에 동일하게 사용하는 구매 가격이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 Price = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.0", Units = "Percent"))
	float IncreasePercent = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 MaxLevel = 10;

	bool IsUpgradeAvailable(int32 Level) const
	{
		return Level >= 0 && Level < MaxLevel;
	}

	float GetStatMultiplier(int32 Level) const
	{
		return static_cast<float>(1.0 + GetTotalIncreasePercent(Level) / 100.0);
	}

	double GetTotalIncreasePercent(int32 Level) const
	{
		return static_cast<double>(FMath::Max(0, Level)) * IncreasePercent;
	}
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRUpgradeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FGameplayTag UpgradeTag;

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	int32 Level = 0;
};
