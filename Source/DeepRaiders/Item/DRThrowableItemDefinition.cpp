
#include "DRThrowableItemDefinition.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

UDRThrowableItemDefinition::UDRThrowableItemDefinition()
{
	Category = EDRItemCategory::Consumable;
	
	ThrowGameplayCueTag = DRGameplayTags::GameplayCue_Item_Throwable_Throw;
	ImpactGameplayCueTag = DRGameplayTags::GameplayCue_Item_Throwable_Impact;
	
	ThrowPresentation.SoundCueTag = DRGameplayTags::GameplayCue_Sound_Item_Throwable_Throw;
	ImpactPresentation.SoundCueTag = DRGameplayTags::GameplayCue_Sound_Item_Throwable_Impact;
}
