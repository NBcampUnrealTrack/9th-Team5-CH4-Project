#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeepRaiders/Item/DRItemTypes.h"
#include "DRWeaponUpgradeTypes.generated.h"

class UTexture2D;

/**
 * 특정 업그레이드 레벨의 결정적인 결과.
 *
 * SetByCallerMagnitude는 증분값이 아니라 해당 레벨에서 GE에 전달할 누적값이다.
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRWeaponUpgradeLevelData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade", meta = (ClampMin = "0", UIMin = "0"))
	int32 Price = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	float SetByCallerMagnitude = 0.0f;
};

/**
 * 하나의 독립적인 무기 스탯 업그레이드 트랙.
 *
 * Levels[0]은 Level 1, Levels[1]은 Level 2에 해당한다.
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRWeaponStatUpgradeData
{
	GENERATED_BODY()

public:
	int32 GetMaxLevel() const
	{
		return Levels.Num();
	}

	bool IsValidTargetLevel(int32 TargetLevel) const
	{
		return TargetLevel > 0 && Levels.IsValidIndex(TargetLevel - 1);
	}

	const FDRWeaponUpgradeLevelData* FindLevelData(int32 TargetLevel) const
	{
		return IsValidTargetLevel(TargetLevel) ? &Levels[TargetLevel - 1] : nullptr;
	}

	/** RuntimeState에서 업그레이드 트랙과 현재 레벨을 식별하는 태그다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	FGameplayTag UpgradeTag;

	/** 장착 GE Spec에 누적값을 전달할 때 사용하는 SetByCaller 태그다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade|UI")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade|UI", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade|UI")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	TArray<FDRWeaponUpgradeLevelData> Levels;
};