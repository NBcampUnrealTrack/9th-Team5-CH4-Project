#include "DRSkillDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UDRSkillDefinition::UDRSkillDefinition()
{
	Category = EDRItemCategory::Skill;
	MaxStackSize = 1;
	bCanBeDropped = false;
	bCanBeSold = false;
}

#if WITH_EDITOR
EDataValidationResult UDRSkillDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!CooldownTag.IsValid())
	{
		Context.AddError(FText::FromString(
			TEXT("Skill Cooldown Tag is required.")));
		Result = EDataValidationResult::Invalid;
	}

	if (CooldownDuration <= 0.0f)
	{
		Context.AddError(FText::FromString(
			TEXT("Skill Cooldown Duration must be greater than zero.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
