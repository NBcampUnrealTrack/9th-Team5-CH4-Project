#include "DRGE_CharacterStatUpgrade.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

UDRGE_CharacterStatUpgrade::UDRGE_CharacterStatUpgrade()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddStat(UDRPlayerAttributeSet::GetMaxHealthAttribute(), DRGameplayTags::Data_Upgrade_MaxHealth);
	AddStat(UDRPlayerAttributeSet::GetMoveSpeedMultiplierAttribute(), DRGameplayTags::Data_Upgrade_MoveSpeed);
}

void UDRGE_CharacterStatUpgrade::AddStat(const FGameplayAttribute& Attribute, FGameplayTag ValueTag)
{
	FSetByCallerFloat Value;
	Value.DataTag = ValueTag;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Value);
}
