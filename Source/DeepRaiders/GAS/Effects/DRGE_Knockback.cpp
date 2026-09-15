#include "DRGE_Knockback.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

UDRGE_Knockback::UDRGE_Knockback(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Distance;
	Distance.DataTag = DRGameplayTags::Data_Knockback_Distance;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UDRPlayerAttributeSet::GetIncomingKnockbackDistanceAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Distance);
}
