#include "DRGE_QuickSlotActivationInterval.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UDRGE_QuickSlotActivationInterval::UDRGE_QuickSlotActivationInterval(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag = DRGameplayTags::Data_QuickSlot_ActivationInterval_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	UTargetTagsGameplayEffectComponent* TargetTagsComponent = 
			ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);

	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}