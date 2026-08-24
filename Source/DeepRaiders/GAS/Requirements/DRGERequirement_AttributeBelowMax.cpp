
#include "DRGERequirement_AttributeBelowMax.h"

#include "AbilitySystemComponent.h"

bool UDRGERequirement_AttributeBelowMax::CanApplyGameplayEffect_Implementation(const UGameplayEffect* GameplayEffect,
	const FGameplayEffectSpec& Spec, UAbilitySystemComponent* ASC) const
{
	if (!IsValid(ASC)
		|| !CurrentAttribute.IsValid()
		|| !MaxAttribute.IsValid()
		|| !ASC->HasAttributeSetForAttribute(CurrentAttribute)
		|| !ASC->HasAttributeSetForAttribute(MaxAttribute))
	{
		return false;
	}
	
	const float CurrentValue = ASC->GetNumericAttribute(CurrentAttribute);
	const float MaxValue = ASC->GetNumericAttribute(MaxAttribute);
	
	return CurrentValue + KINDA_SMALL_NUMBER < MaxValue;	
}
