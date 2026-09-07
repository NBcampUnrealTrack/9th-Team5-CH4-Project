#include "DRGE_SkillCooldown.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UDRGE_SkillCooldown::UDRGE_SkillCooldown(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 모든 개별 스킬이 공용으로 사용하는 쿨다운 GE 설정이다.
	// 실제 태그는 Ability Spec의 DynamicGrantedTags로 추가한다.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag = DRGameplayTags::Data_Cooldown_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
}

UDRGE_SkillChargeCooldown::UDRGE_SkillChargeCooldown(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDRGE_SkillCooldown::ConfigureCooldown(
	const FObjectInitializer& ObjectInitializer,
	FGameplayTag CooldownTag)
{
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);

	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(CooldownTag);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

UDRGE_SkillOneCooldown::UDRGE_SkillOneCooldown(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureCooldown(ObjectInitializer, DRGameplayTags::Cooldown_Skill_One);
}

UDRGE_SkillTwoCooldown::UDRGE_SkillTwoCooldown(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureCooldown(ObjectInitializer, DRGameplayTags::Cooldown_Skill_Two);
}
