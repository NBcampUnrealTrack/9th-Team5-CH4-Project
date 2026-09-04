#include "DRCharacterUpgradeProfile.h"

#include "DRGE_CharacterStatUpgrade.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

UDRCharacterUpgradeProfile::UDRCharacterUpgradeProfile()
{
	EffectClass = UDRGE_CharacterStatUpgrade::StaticClass();
	AddStat(DRGameplayTags::Character_Upgrade_MaxHealth, DRGameplayTags::Data_Upgrade_MaxHealth,
		NSLOCTEXT("CharacterUpgrade", "MaxHealth", "최대 체력"));
	AddStat(DRGameplayTags::Character_Upgrade_MoveSpeed, DRGameplayTags::Data_Upgrade_MoveSpeed,
		NSLOCTEXT("CharacterUpgrade", "MoveSpeed", "이동 속도"));
}

void UDRCharacterUpgradeProfile::AddStat(FGameplayTag UpgradeTag, FGameplayTag ValueTag, const FText& DisplayName)
{
	FDRStatUpgradeData& Data = StatUpgrades.AddDefaulted_GetRef();
	Data.UpgradeTag = UpgradeTag;
	Data.SetByCallerTag = ValueTag;
	Data.DisplayName = DisplayName;
}

bool UDRCharacterUpgradeProfile::IsUsable() const
{
	const UGameplayEffect* Effect = EffectClass.GetDefaultObject();
	// UE 5.7의 GetStackingType은 모듈 외부로 export되지 않는다.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const bool IsStacking = Effect && Effect->StackingType != EGameplayEffectStackingType::None;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
		|| IsStacking
		|| Effect->Period.GetValueAtLevel(1.f) != 0.f || !Effect->Executions.IsEmpty()
		|| StatUpgrades.IsEmpty())
	{
		return false;
	}

	TSet<FGameplayTag> EffectTags;
	for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
	{
		const FGameplayTag Tag = Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag;
		if (!Modifier.Attribute.IsValid() || Modifier.ModifierOp != EGameplayModOp::Multiplicitive
			|| Modifier.ModifierMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::SetByCaller
			|| !Tag.IsValid())
		{
			return false;
		}
		EffectTags.Add(Tag);
	}

	TSet<FGameplayTag> UpgradeTags;
	TSet<FGameplayTag> ValueTags;
	for (const FDRStatUpgradeData& Data : StatUpgrades)
	{
		if (!Data.UpgradeTag.IsValid() || !EffectTags.Contains(Data.SetByCallerTag)
			|| UpgradeTags.Contains(Data.UpgradeTag) || ValueTags.Contains(Data.SetByCallerTag)
			|| Data.Price < 0 || Data.MaxLevel < 0 || !FMath::IsFinite(Data.IncreasePercent) || Data.IncreasePercent < 0.f)
		{
			return false;
		}
		UpgradeTags.Add(Data.UpgradeTag);
		ValueTags.Add(Data.SetByCallerTag);
	}
	return true;
}
