#include "DRGA_CharacterSkillBase.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/Effects/DRGE_SkillCooldown.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UDRGA_CharacterSkillBase::UDRGA_CharacterSkillBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
}

UGameplayEffect* UDRGA_CharacterSkillBase::GetCooldownGameplayEffect() const
{
	const FGameplayAbilitySpec* AbilitySpec = IsInstantiated()
		? GetCurrentAbilitySpec()
		: nullptr;
	const TSubclassOf<UGameplayEffect> CooldownClass = AbilitySpec != nullptr
		&& AbilitySpec->InputID == static_cast<int32>(EDRAbilityInputId::Skill2)
		? UDRGE_SkillTwoCooldown::StaticClass()
		: UDRGE_SkillOneCooldown::StaticClass();

	return CooldownClass->GetDefaultObject<UGameplayEffect>();
}

#if WITH_EDITOR
EDataValidationResult UDRGA_CharacterSkillBase::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const bool IsSkillOneCooldownValid = IsCooldownEffectValid(
		UDRGE_SkillOneCooldown::StaticClass()->GetDefaultObject<UGameplayEffect>(),
		DRGameplayTags::Cooldown_Skill_One,
		Context);
	const bool IsSkillTwoCooldownValid = IsCooldownEffectValid(
		UDRGE_SkillTwoCooldown::StaticClass()->GetDefaultObject<UGameplayEffect>(),
		DRGameplayTags::Cooldown_Skill_Two,
		Context);

	if (!IsSkillOneCooldownValid || !IsSkillTwoCooldownValid)
	{
		Result = EDataValidationResult::Invalid;
	}

	if (CooldownDuration <= 0.0f)
	{
		Context.AddError(FText::FromString(
			TEXT("Cooldown Duration must be greater than zero.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

bool UDRGA_CharacterSkillBase::IsCooldownEffectValid(
	const UGameplayEffect* CooldownEffect,
	FGameplayTag ExpectedCooldownTag,
	FDataValidationContext& Context)
{
	if (!IsValid(CooldownEffect))
	{
		Context.AddError(FText::FromString(
			TEXT("Skill cooldown Gameplay Effect is invalid.")));
		return false;
	}

	const FGameplayEffectModifierMagnitude& DurationMagnitude =
		CooldownEffect->DurationMagnitude;
	const bool IsDurationValid =
		DurationMagnitude.GetMagnitudeCalculationType()
			== EGameplayEffectMagnitudeCalculation::SetByCaller
		&& DurationMagnitude.GetSetByCallerFloat().DataTag
			== DRGameplayTags::Data_Cooldown_Duration;
	const bool IsEffectValid =
		CooldownEffect->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& CooldownEffect->GetGrantedTags().HasTagExact(ExpectedCooldownTag)
		&& IsDurationValid;

	if (!IsEffectValid)
	{
		Context.AddError(FText::Format(
			FText::FromString(TEXT("Skill cooldown Gameplay Effect for '{0}' is invalid.")),
			FText::FromName(ExpectedCooldownTag.GetTagName())));
	}

	return IsEffectValid;
}
#endif

bool UDRGA_CharacterSkillBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return IsValid(GetPlayerCharacter(ActorInfo))
		&& ActorInfo != nullptr
		&& IsValid(Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()));
}

void UDRGA_CharacterSkillBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (ActorInfo == nullptr
		|| CooldownDuration <= 0.0f)
	{
		return;
	}

	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		GetCooldownGameplayEffect()->GetClass(),
		GetAbilityLevel(Handle, ActorInfo));

	if (!CooldownSpec.IsValid())
	{
		return;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Cooldown_Duration,
		CooldownDuration);

	ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownSpec);
}

ADRPlayerCharacter* UDRGA_CharacterSkillBase::GetPlayerCharacter(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo != nullptr
		? Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
}

