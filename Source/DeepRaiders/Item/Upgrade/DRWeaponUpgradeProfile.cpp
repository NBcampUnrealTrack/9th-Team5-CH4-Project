#include "DRWeaponUpgradeProfile.h"

#include "DRGE_WeaponStatUpgrade.h"
#include "GameplayEffect.h"

UDRWeaponUpgradeProfile::UDRWeaponUpgradeProfile()
{
	EquippedEffectClass = UDRGE_WeaponStatUpgrade::StaticClass();
}

const FDRWeaponStatUpgradeData* UDRWeaponUpgradeProfile::FindStatUpgrade(const FGameplayTag& UpgradeTag) const
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

const FDRWeaponUpgradeLevelData* UDRWeaponUpgradeProfile::FindLevelData(const FGameplayTag& UpgradeTag,
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

bool UDRWeaponUpgradeProfile::IsUsable() const
{
	const UGameplayEffect* Effect = EquippedEffectClass.GetDefaultObject();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const bool bUsesStacking = Effect != nullptr && Effect->StackingType != EGameplayEffectStackingType::None;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (Effect == nullptr
		|| Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
		|| bUsesStacking
		|| Effect->Period.GetValueAtLevel(1.0f) != 0.0f
		|| !Effect->Executions.IsEmpty()
		|| StatUpgrades.IsEmpty())
	{
		return false;
	}

	TSet<FGameplayTag> EffectValueTags;

	for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
	{
		const FGameplayTag ValueTag = Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag;

		if (!Modifier.Attribute.IsValid()
			|| Modifier.ModifierOp != EGameplayModOp::Multiplicitive
			|| Modifier.ModifierMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::SetByCaller
			|| !ValueTag.IsValid())
		{
			return false;
		}

		EffectValueTags.Add(ValueTag);
	}

	TSet<FGameplayTag> UpgradeTags;
	TSet<FGameplayTag> ProfileValueTags;

	for (const FDRWeaponStatUpgradeData& UpgradeData : StatUpgrades)
	{
		if (!UpgradeData.UpgradeTag.IsValid()
			|| !EffectValueTags.Contains(UpgradeData.SetByCallerTag)
			|| UpgradeTags.Contains(UpgradeData.UpgradeTag)
			|| ProfileValueTags.Contains(UpgradeData.SetByCallerTag)
			|| UpgradeData.Levels.IsEmpty())
		{
			return false;
		}

		for (const FDRWeaponUpgradeLevelData& LevelData : UpgradeData.Levels)
		{
			if (LevelData.Price < 0
				|| !FMath::IsFinite(LevelData.SetByCallerMagnitude)
				|| LevelData.SetByCallerMagnitude < 0.0f)
			{
				return false;
			}
		}

		UpgradeTags.Add(UpgradeData.UpgradeTag);
		ProfileValueTags.Add(UpgradeData.SetByCallerTag);
	}

	return true;
}
