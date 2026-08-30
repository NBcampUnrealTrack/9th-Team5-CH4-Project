#include "DRGA_CharacterSkillBase.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Effects/DRGE_SkillCooldown.h"
#include "AbilitySystemComponent.h"
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
	ActivationBlockedTags.AddTag(DRGameplayTags::State_BlinkRecovery);
}

UGameplayEffect* UDRGA_CharacterSkillBase::GetCooldownGameplayEffect() const
{
	return UDRGE_SkillCooldown::StaticClass()->GetDefaultObject<UGameplayEffect>();
}

const FGameplayTagContainer* UDRGA_CharacterSkillBase::GetCooldownTags() const
{
	CurrentCooldownTags.Reset();
	const FGameplayTag CooldownTag = GetCooldownTag();
	if (CooldownTag.IsValid())
	{
		CurrentCooldownTags.AddTag(CooldownTag);
	}

	return &CurrentCooldownTags;
}

#if WITH_EDITOR
EDataValidationResult UDRGA_CharacterSkillBase::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!IsCooldownEffectValid(
		UDRGE_SkillCooldown::StaticClass()->GetDefaultObject<UGameplayEffect>(),
		Context))
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
		&& IsDurationValid;

	if (!IsEffectValid)
	{
		Context.AddError(FText::FromString(
			TEXT("Shared skill cooldown Gameplay Effect is invalid.")));
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

	// CanActivate 시점에는 GetCurrentAbilitySpec()이 아직 현재 Handle을 가리키지
	// 않을 수 있다. 전달된 Handle의 SourceObject에서 개별 쿨다운 태그를 직접
	// 검사해 동일 스킬의 연속 사용을 막는다.
	const UDRSkillDefinition* SkillDefinition = ActorInfo != nullptr
		? Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo))
		: nullptr;
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (IsValid(SkillDefinition)
		&& SkillDefinition->CooldownTag.IsValid()
		&& IsValid(AbilitySystemComponent)
		&& AbilitySystemComponent->HasMatchingGameplayTag(SkillDefinition->CooldownTag))
	{
		if (OptionalRelevantTags != nullptr)
		{
			OptionalRelevantTags->AddTag(SkillDefinition->CooldownTag);
		}
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
	// Commit 중인 Spec의 SourceObject를 직접 읽어, 현재 활성 Spec 조회 실패 시
	// 슬롯 기반 호환 쿨다운으로 잘못 폴백하지 않도록 한다.
	const UDRSkillDefinition* SkillDefinition =
		Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	const float Cooldown =
		IsValid(SkillDefinition) && SkillDefinition->CooldownDuration > 0.0f
		? SkillDefinition->CooldownDuration
		: GetCooldownDuration();
	const FGameplayTag CooldownTag =
		IsValid(SkillDefinition) && SkillDefinition->CooldownTag.IsValid()
		? SkillDefinition->CooldownTag
		: GetCooldownTag();
	if (ActorInfo == nullptr
		|| Cooldown <= 0.0f
		|| !CooldownTag.IsValid())
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
		Cooldown);
	CooldownSpec.Data->DynamicGrantedTags.AddTag(CooldownTag);

	ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownSpec);

	NotifySkillCommitted(Handle, ActorInfo);
}

ADRPlayerCharacter* UDRGA_CharacterSkillBase::GetPlayerCharacter(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo != nullptr
		? Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
}

const UDRSkillDefinition* UDRGA_CharacterSkillBase::GetCurrentSkillDefinition() const
{
	const FGameplayAbilitySpec* AbilitySpec = IsInstantiated()
		? GetCurrentAbilitySpec()
		: nullptr;
	return AbilitySpec != nullptr
		? Cast<UDRSkillDefinition>(AbilitySpec->SourceObject.Get())
		: nullptr;
}

FGameplayTag UDRGA_CharacterSkillBase::GetCooldownTag() const
{
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	if (IsValid(SkillDefinition) && SkillDefinition->CooldownTag.IsValid())
	{
		return SkillDefinition->CooldownTag;
	}

	const FGameplayAbilitySpec* AbilitySpec = IsInstantiated()
		? GetCurrentAbilitySpec()
		: nullptr;
	return AbilitySpec != nullptr
		&& AbilitySpec->InputID == static_cast<int32>(EDRAbilityInputId::Skill2)
		? DRGameplayTags::Cooldown_Skill_Two
		: DRGameplayTags::Cooldown_Skill_One;
}

float UDRGA_CharacterSkillBase::GetCooldownDuration() const
{
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	return IsValid(SkillDefinition) && SkillDefinition->CooldownDuration > 0.0f
		? SkillDefinition->CooldownDuration
		: CooldownDuration;
}

void UDRGA_CharacterSkillBase::NotifySkillCommitted(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const UDRSkillDefinition* SkillDefinition =
		Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	ADRPlayerState* PlayerState =
		Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get());
	UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;

	if (IsValid(SkillDefinition)
		&& SkillDefinition->SkillId.IsValid()
		&& IsValid(PerkComponent))
	{
		PerkComponent->HandleSkillCommitted(SkillDefinition->SkillId);
	}
}
