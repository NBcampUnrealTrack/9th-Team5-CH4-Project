#include "DRUpgradeProfile.h"

const FDRStatUpgradeData* UDRUpgradeProfile::FindStatUpgrade(FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() ? StatUpgrades.FindByPredicate(
		[UpgradeTag](const FDRStatUpgradeData& Data)
		{
			return Data.UpgradeTag == UpgradeTag;
		}) : nullptr;
}
