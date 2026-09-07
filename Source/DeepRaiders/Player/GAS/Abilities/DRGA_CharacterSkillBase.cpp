#include "DRGA_CharacterSkillBase.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
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

	FGameplayTagContainer InitialAbilityTags;
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	SetAssetTags(InitialAbilityTags);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
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

	return IsValid(GetPlayerCharacter(ActorInfo))
		&& ActorInfo != nullptr
		&& IsValid(Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()));
}

void UDRGA_CharacterSkillBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UDRSkillDefinition* SkillDefinition =
		Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	if (ActorInfo == nullptr
		|| !IsValid(SkillDefinition)
		|| SkillDefinition->CooldownDuration <= 0.0f
		|| !SkillDefinition->CooldownTag.IsValid())
	{
		return;
	}

	ApplyCooldownTag(
		Handle,
		ActorInfo,
		ActivationInfo,
		SkillDefinition->CooldownTag,
		SkillDefinition->CooldownDuration);

	NotifySkillCommitted(Handle, ActorInfo);
	NotifySkillActivated(Handle, ActorInfo);
}

void UDRGA_CharacterSkillBase::ApplyCooldownTag(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTag CooldownTag,
	float Duration) const
{
	if (ActorInfo == nullptr || !CooldownTag.IsValid() || Duration <= 0.f)
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
		Duration);
	CooldownSpec.Data->DynamicGrantedTags.AddTag(CooldownTag);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
}

void UDRGA_CharacterSkillBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (ActorInfo != nullptr)
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get())
		{
			for (const FActiveGameplayEffectHandle EffectHandle : ActiveSkillEffectHandles)
			{
				if (EffectHandle.IsValid())
				{
					AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
				}
			}
		}
	}
	ActiveSkillEffectHandles.Reset();

	if (!bWasCancelled)
	{
		NotifySkillCompleted(Handle, ActorInfo);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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

	return FGameplayTag();
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
		PerkComponent->HandleSkillCommitted(SkillDefinition);
	}
}

void UDRGA_CharacterSkillBase::NotifySkillActivated(
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
	if (IsValid(SkillDefinition) && IsValid(PerkComponent))
	{
		PerkComponent->HandleSkillActivated(
			SkillDefinition,
			ActiveSkillEffectHandles);
	}
}

void UDRGA_CharacterSkillBase::NotifySkillCompleted(
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
	if (IsValid(SkillDefinition) && IsValid(PerkComponent))
	{
		PerkComponent->HandleSkillCompleted(SkillDefinition);
	}
}
