#include "DRGA_CharacterSkillBase.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Effects/DRGE_SkillCooldown.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UDRGA_CharacterSkillBase::UDRGA_CharacterSkillBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer InitialAbilityTags;
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Action);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	SetAssetTags(InitialAbilityTags);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_BlinkRecovery);
}

UGameplayEffect* UDRGA_CharacterSkillBase::GetCooldownGameplayEffect() const
{
	const FGameplayAbilitySpec* AbilitySpec = IsInstantiated()
		? GetCurrentAbilitySpec()
		: nullptr;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (AbilitySpec != nullptr
		&& UsesCharges(AbilitySpec->Handle, ActorInfo))
	{
		return UDRGE_SkillChargeCooldown::StaticClass()
			->GetDefaultObject<UGameplayEffect>();
	}

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

bool UDRGA_CharacterSkillBase::CheckCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	int32 MaxCharges = 0;
	if (!UsesCharges(Handle, ActorInfo, &MaxCharges))
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	}

	const UDRSkillDefinition* SkillDefinition = ActorInfo != nullptr
		? Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo))
		: nullptr;
	if (!IsValid(SkillDefinition))
	{
		return false;
	}

	const bool bHasAvailableCharge = GetConsumedChargeCount(
		ActorInfo,
		SkillDefinition->CooldownTag) < MaxCharges;
	if (!bHasAvailableCharge && OptionalRelevantTags != nullptr)
	{
		OptionalRelevantTags->AddTag(SkillDefinition->CooldownTag);
	}

	return bHasAvailableCharge;
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
	if (!IsCooldownEffectValid(
		UDRGE_SkillChargeCooldown::StaticClass()->GetDefaultObject<UGameplayEffect>(),
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

	// CanActivate 시점에는 GetCurrentAbilitySpec()이 아직 현재 Handle을 가리키지
	// 않을 수 있다. 전달된 Handle의 SourceObject에서 개별 쿨다운 태그를 직접
	// 검사해 동일 스킬의 연속 사용을 막는다.
	const UDRSkillDefinition* SkillDefinition = ActorInfo != nullptr
		? Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo))
		: nullptr;
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	int32 MaxCharges = 0;
	const bool bUsesCharges = UsesCharges(Handle, ActorInfo, &MaxCharges);
	if (IsValid(SkillDefinition)
		&& SkillDefinition->CooldownTag.IsValid()
		&& IsValid(AbilitySystemComponent)
		&& ((!bUsesCharges
				&& AbilitySystemComponent->HasMatchingGameplayTag(SkillDefinition->CooldownTag))
			|| (bUsesCharges
				&& GetConsumedChargeCount(ActorInfo, SkillDefinition->CooldownTag)
					>= MaxCharges)))
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
	const UDRSkillDefinition* SkillDefinition =
		Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	if (ActorInfo == nullptr
		|| !IsValid(SkillDefinition)
		|| SkillDefinition->CooldownDuration <= 0.0f
		|| !SkillDefinition->CooldownTag.IsValid())
	{
		return;
	}

	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		UsesCharges(Handle, ActorInfo)
			? UDRGE_SkillChargeCooldown::StaticClass()
			: UDRGE_SkillCooldown::StaticClass(),
		GetAbilityLevel(Handle, ActorInfo));

	if (!CooldownSpec.IsValid())
	{
		return;
	}

	const bool bUsesCharges = UsesCharges(Handle, ActorInfo);
	const float EffectiveCooldownDuration = bUsesCharges
		? SkillDefinition->CooldownDuration + GetChargeQueueTailRemaining(
			ActorInfo,
			SkillDefinition->CooldownTag)
		: SkillDefinition->CooldownDuration;

	CooldownSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Cooldown_Duration,
		EffectiveCooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AddTag(
		SkillDefinition->CooldownTag);

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
	const FString ActivationPredictionKey = ActivationInfo.GetActivationPredictionKey().ToString();
	const FString ScopedPredictionKey = IsValid(AbilitySystemComponent)
		? AbilitySystemComponent->GetPredictionKeyForNewAction().ToString()
		: TEXT("None");
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[SkillCooldown][Apply] Skill=%s Tag=%s Authority=%d Local=%d Duration=%.2f ActivationKey=%s ScopedKey=%s"),
		*SkillDefinition->SkillId.ToString(),
		*SkillDefinition->CooldownTag.ToString(),
		ActorInfo->IsNetAuthority(),
		ActorInfo->IsLocallyControlled(),
		EffectiveCooldownDuration,
		*ActivationPredictionKey,
		*ScopedPredictionKey);

	const FActiveGameplayEffectHandle AppliedCooldownHandle =
		ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownSpec);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[SkillCooldown][Applied] Skill=%s Tag=%s Authority=%d Handle=%s Applied=%d"),
		*SkillDefinition->SkillId.ToString(),
		*SkillDefinition->CooldownTag.ToString(),
		ActorInfo->IsNetAuthority(),
		*AppliedCooldownHandle.ToString(),
		AppliedCooldownHandle.WasSuccessfullyApplied());
	if (AppliedCooldownHandle.WasSuccessfullyApplied())
	{
		ScheduleCooldownSafetyCleanup(
			ActorInfo,
			AppliedCooldownHandle,
			SkillDefinition->CooldownTag,
			EffectiveCooldownDuration);
	}

	NotifySkillCommitted(Handle, ActorInfo);
	NotifySkillActivated(Handle, ActorInfo);
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

bool UDRGA_CharacterSkillBase::UsesCharges(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	int32* OutMaxCharges) const
{
	if (OutMaxCharges != nullptr)
	{
		*OutMaxCharges = 1;
	}

	const UDRSkillDefinition* SkillDefinition = ActorInfo != nullptr
		? Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo))
		: nullptr;
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;
	if (!IsValid(SkillDefinition)
		|| !SkillDefinition->SkillId.IsValid()
		|| !IsValid(PerkComponent)
		|| !PerkComponent->HasSkillPerk(
			SkillDefinition->SkillId,
			DRGameplayTags::Perk_Skill_Charges))
	{
		return false;
	}

	const float ConfiguredMaxCharges =
		PerkComponent->GetSkillPerkEffectValue(
			SkillDefinition->SkillId,
			DRGameplayTags::Perk_Skill_Charges,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Perk_Charges_Max);
	const int32 MaxCharges = ConfiguredMaxCharges > 0.0f
		? FMath::Max(1, FMath::RoundToInt(ConfiguredMaxCharges))
		: 3;
	if (OutMaxCharges != nullptr)
	{
		*OutMaxCharges = MaxCharges;
	}

	return MaxCharges > 1;
}

int32 UDRGA_CharacterSkillBase::GetConsumedChargeCount(
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTag CooldownTag) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystemComponent) || !CooldownTag.IsValid())
	{
		return 0;
	}

	FGameplayTagContainer CooldownTags(CooldownTag);
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	int32 ConsumedCharges = 0;
	for (const FActiveGameplayEffectHandle EffectHandle
		: AbilitySystemComponent->GetActiveEffects(Query))
	{
		ConsumedCharges += AbilitySystemComponent->GetCurrentStackCount(EffectHandle);
	}

	return ConsumedCharges;
}

float UDRGA_CharacterSkillBase::GetChargeQueueTailRemaining(
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTag CooldownTag) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystemComponent) || !CooldownTag.IsValid())
	{
		return 0.f;
	}

	FGameplayTagContainer CooldownTags(CooldownTag);
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	float QueueTailRemaining = 0.f;
	for (const TPair<float, float>& TimeAndDuration
		: AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		QueueTailRemaining = FMath::Max(
			QueueTailRemaining,
			TimeAndDuration.Key);
	}

	return QueueTailRemaining;
}

void UDRGA_CharacterSkillBase::ScheduleCooldownSafetyCleanup(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FActiveGameplayEffectHandle CooldownEffectHandle,
	const FGameplayTag CooldownTag,
	const float CooldownDuration) const
{
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !CooldownEffectHandle.IsValid()
		|| !CooldownTag.IsValid()
		|| CooldownDuration <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo->AbilitySystemComponent.Get();
	UWorld* World = ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->GetWorld()
		: nullptr;
	if (!IsValid(AbilitySystemComponent) || !IsValid(World))
	{
		return;
	}

	// 정상적으로는 GE의 Duration이 태그를 회수한다. 서버 지연이나 취소 경로로
	// GE가 남은 경우를 대비해, 쿨다운 종료 시점보다 조금 뒤에 해당 Handle만
	// 강제로 회수한다. 다른 스킬/차지의 쿨다운은 건드리지 않는다.
	constexpr float CleanupGraceSeconds = 0.25f;
	const TWeakObjectPtr<UAbilitySystemComponent> WeakAbilitySystem =
		AbilitySystemComponent;
	FTimerDelegate CleanupDelegate;
	CleanupDelegate.BindLambda([
		WeakAbilitySystem,
		CooldownEffectHandle,
		CooldownTag]()
	{
		if (UAbilitySystemComponent* ASC = WeakAbilitySystem.Get())
		{
			const bool bEffectStillActive = ASC->GetActiveGameplayEffect(CooldownEffectHandle) != nullptr;
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[SkillCooldown][SafetyCleanup] Tag=%s Handle=%s Active=%d TagPresent=%d"),
				*CooldownTag.ToString(),
				*CooldownEffectHandle.ToString(),
				bEffectStillActive,
				ASC->HasMatchingGameplayTag(CooldownTag));
			if (bEffectStillActive)
			{
				ASC->RemoveActiveGameplayEffect(CooldownEffectHandle);
			}
		}
	});

	FTimerHandle CleanupTimer;
	World->GetTimerManager().SetTimer(
		CleanupTimer,
		CleanupDelegate,
		CooldownDuration + CleanupGraceSeconds,
		false);
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
