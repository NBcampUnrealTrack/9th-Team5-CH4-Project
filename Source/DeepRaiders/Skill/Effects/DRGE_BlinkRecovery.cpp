#include "DRGE_BlinkRecovery.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UDRGE_BlinkRecovery::UDRGE_BlinkRecovery(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag = DRGameplayTags::Data_BlinkRecovery_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);

	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(DRGameplayTags::State_BlinkRecovery);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
