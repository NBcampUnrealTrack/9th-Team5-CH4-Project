#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DeepRaiders/Item/Upgrade/DRWeaponUpgradeTypes.h"
#include "DRWeaponUpgradeProfile.generated.h"

class UGameplayEffect;

/**
 * 한 종류의 눈 Projectile Weapon이 제공하는 스탯별 업그레이드 정의.
 *
 * ItemDefinition이 이 Profile을 참조하며, 상점과 장착 Effect 생성 코드가
 * 동일한 Profile을 데이터 원본으로 사용한다.
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRWeaponUpgradeProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FDRWeaponStatUpgradeData>& GetStatUpgrades() const
	{
		return StatUpgrades;
	}

	const FDRWeaponStatUpgradeData* FindStatUpgrade(const FGameplayTag& UpgradeTag) const;

	const FDRWeaponUpgradeLevelData* FindLevelData(const FGameplayTag& UpgradeTag, int32 TargetLevel) const;

	int32 GetMaxLevel(const FGameplayTag& UpgradeTag) const;

	/**
	 * RuntimeState를 장착 중인 ASC 상태로 투영하는 Infinite GameplayEffect.
	 * 실제 SetByCaller 값은 장착 시 RuntimeState와 StatUpgrades를 기반으로 설정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade|GAS")
	TSubclassOf<UGameplayEffect> EquippedEffectClass;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	TArray<FDRWeaponStatUpgradeData> StatUpgrades;
};