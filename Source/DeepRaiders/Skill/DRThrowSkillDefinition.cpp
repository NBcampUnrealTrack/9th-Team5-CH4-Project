#include "DRThrowSkillDefinition.h"

#include "DeepRaiders/Item/DRThrowableItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UDRThrowSkillDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!IsValid(ThrowableDefinition))
	{
		Context.AddError(FText::FromString(TEXT("Throw Skill requires a Throwable Definition.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
