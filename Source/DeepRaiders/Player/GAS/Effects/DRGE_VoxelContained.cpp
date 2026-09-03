#include "DRGE_VoxelContained.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UDRGE_VoxelContained::UDRGE_VoxelContained(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);

	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(DRGameplayTags::State_VoxelContained);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
