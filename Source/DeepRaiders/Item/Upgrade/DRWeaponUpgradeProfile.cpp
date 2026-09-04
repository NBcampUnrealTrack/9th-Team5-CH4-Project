#include "DRWeaponUpgradeProfile.h"

const FDRWeaponStatUpgradeData* UDRWeaponUpgradeProfile::FindStatUpgrade(
	const FGameplayTag& UpgradeTag) const
{
	if (!UpgradeTag.IsValid())
	{
		return nullptr;
	}

	return StatUpgrades.FindByPredicate(
		[UpgradeTag](const FDRWeaponStatUpgradeData& UpgradeData)
		{
			return UpgradeData.UpgradeTag == UpgradeTag;
		});
}

const FDRWeaponUpgradeLevelData* UDRWeaponUpgradeProfile::FindLevelData(
	const FGameplayTag& UpgradeTag,
	int32 TargetLevel) const
{
	const FDRWeaponStatUpgradeData* UpgradeData = FindStatUpgrade(UpgradeTag);
	return UpgradeData != nullptr ? UpgradeData->FindLevelData(TargetLevel) : nullptr;
}

int32 UDRWeaponUpgradeProfile::GetMaxLevel(const FGameplayTag& UpgradeTag) const
{
	const FDRWeaponStatUpgradeData* UpgradeData = FindStatUpgrade(UpgradeTag);
	return UpgradeData != nullptr ? UpgradeData->GetMaxLevel() : 0;
}