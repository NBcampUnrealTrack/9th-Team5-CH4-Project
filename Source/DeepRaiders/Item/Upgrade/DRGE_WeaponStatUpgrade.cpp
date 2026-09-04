#include "DRGE_WeaponStatUpgrade.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

UDRGE_WeaponStatUpgrade::UDRGE_WeaponStatUpgrade()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	AddStat(UDRPlayerAttributeSet::GetWeaponDamageMultiplierAttribute(),
		DRGameplayTags::Data_Weapon_DamageModifier);
	AddStat(UDRPlayerAttributeSet::GetWeaponFireIntervalMultiplierAttribute(),
		DRGameplayTags::Data_Weapon_FireIntervalModifier);
	AddStat(UDRPlayerAttributeSet::GetWeaponSnowCostMultiplierAttribute(),
		DRGameplayTags::Data_Weapon_SnowCostModifier);
	AddStat(UDRPlayerAttributeSet::GetWeaponProjectileCountMultiplierAttribute(),
		DRGameplayTags::Data_Weapon_ProjectileCountModifier);
}

void UDRGE_WeaponStatUpgrade::AddStat(const FGameplayAttribute& Attribute, FGameplayTag ValueTag)
{
	FSetByCallerFloat Value;
	Value.DataTag = ValueTag;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Value);
}
