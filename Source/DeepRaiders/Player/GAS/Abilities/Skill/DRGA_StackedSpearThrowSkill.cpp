#include "DRGA_StackedSpearThrowSkill.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Effects/DRGE_SkillCooldown.h"

UDRGA_StackedSpearThrowSkill::UDRGA_StackedSpearThrowSkill()
{
	MaxAimDistance = 500.f;
	ProjectileSpeed = 3500.f;
}

void UDRGA_StackedSpearThrowSkill::OnGiveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	CurrentStackCount = 1;
	BindRechargeCooldown(ActorInfo, Spec);
}

void UDRGA_StackedSpearThrowSkill::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	UnbindRechargeCooldown();
	CurrentStackCount = 0;
	Super::OnRemoveAbility(ActorInfo, Spec);
}

bool UDRGA_StackedSpearThrowSkill::CheckCooldown(
	const FGameplayAbilitySpecHandle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystemComponent))
	{
		return false;
	}

	FGameplayTag BlockedTag;
	if (CurrentStackCount <= 0)
	{
		BlockedTag = RechargeCooldownTag;
	}
	else if (AbilitySystemComponent->HasMatchingGameplayTag(
		DRGameplayTags::Cooldown_Skill_SpearThrow_FireInterval))
	{
		BlockedTag = DRGameplayTags::Cooldown_Skill_SpearThrow_FireInterval;
	}

	if (!BlockedTag.IsValid())
	{
		return true;
	}

	if (OptionalRelevantTags != nullptr)
	{
		OptionalRelevantTags->AddTag(BlockedTag);
	}
	return false;
}

void UDRGA_StackedSpearThrowSkill::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (CurrentStackCount <= 0
		|| !RechargeCooldownTag.IsValid()
		|| RechargeCooldownDuration <= 0.f
		|| !IsValid(AbilitySystemComponent))
	{
		return;
	}

	const bool IsRechargeActive = AbilitySystemComponent->HasMatchingGameplayTag(
		RechargeCooldownTag);
	--CurrentStackCount;
	StackChangedDelegate.Broadcast();
	NotifySkillCommitted(Handle, ActorInfo);
	NotifySkillActivated(Handle, ActorInfo);
	if (!IsRechargeActive)
	{
		ApplyCooldownTag(
			Handle,
			ActorInfo,
			ActivationInfo,
			RechargeCooldownTag,
			RechargeCooldownDuration);
	}

	ApplyCooldownTag(
		Handle,
		ActorInfo,
		ActivationInfo,
		DRGameplayTags::Cooldown_Skill_SpearThrow_FireInterval,
		FMath::Max(FireInterval, 0.f));
}

void UDRGA_StackedSpearThrowSkill::BindRechargeCooldown(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	UnbindRechargeCooldown();

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	const UDRSkillDefinition* SkillDefinition = Cast<UDRSkillDefinition>(
		Spec.SourceObject.Get());
	if (!IsValid(AbilitySystemComponent)
		|| !IsValid(SkillDefinition)
		|| !SkillDefinition->CooldownTag.IsValid()
		|| SkillDefinition->CooldownDuration <= 0.f)
	{
		return;
	}

	RechargeAbilitySystemComponent = AbilitySystemComponent;
	RechargeCooldownTag = SkillDefinition->CooldownTag;
	RechargeCooldownDuration = SkillDefinition->CooldownDuration;
	RechargeCooldownTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
		RechargeCooldownTag,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ThisClass::HandleRechargeCooldownTagChanged);
}

void UDRGA_StackedSpearThrowSkill::UnbindRechargeCooldown()
{
	UAbilitySystemComponent* AbilitySystemComponent = RechargeAbilitySystemComponent.Get();
	if (IsValid(AbilitySystemComponent)
		&& RechargeCooldownTag.IsValid()
		&& RechargeCooldownTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			RechargeCooldownTag,
			EGameplayTagEventType::NewOrRemoved).Remove(
				RechargeCooldownTagChangedHandle);
	}

	RechargeCooldownTagChangedHandle.Reset();
	RechargeAbilitySystemComponent.Reset();
	RechargeCooldownTag = FGameplayTag();
	RechargeCooldownDuration = 0.f;
}

void UDRGA_StackedSpearThrowSkill::HandleRechargeCooldownTagChanged(
	FGameplayTag,
	int32 NewCount)
{
	const int32 MaximumCount = GetMaximumStackCount();
	if (NewCount > 0 || CurrentStackCount >= MaximumCount)
	{
		return;
	}

	++CurrentStackCount;
	StackChangedDelegate.Broadcast();

	if (CurrentStackCount < MaximumCount)
	{
		StartRechargeCooldown();
	}
}

void UDRGA_StackedSpearThrowSkill::StartRechargeCooldown()
{
	UAbilitySystemComponent* AbilitySystemComponent = RechargeAbilitySystemComponent.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsValid(AbilitySystemComponent)
		|| ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle CooldownSpec = AbilitySystemComponent->MakeOutgoingSpec(
		UDRGE_SkillCooldown::StaticClass(),
		1.f,
		EffectContext);
	if (!CooldownSpec.IsValid())
	{
		return;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Cooldown_Duration,
		RechargeCooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AddTag(RechargeCooldownTag);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
}

int32 UDRGA_StackedSpearThrowSkill::GetMaximumStackCount() const
{
	return FMath::Max(MaximumStackCount, 1);
}

int32 UDRGA_StackedSpearThrowSkill::GetCurrentStackCount() const
{
	return CurrentStackCount;
}

FDRSpearStackChangedSignature& UDRGA_StackedSpearThrowSkill::GetStackChangedDelegate()
{
	return StackChangedDelegate;
}
