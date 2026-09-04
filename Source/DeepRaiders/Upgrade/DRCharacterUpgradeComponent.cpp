#include "DRCharacterUpgradeComponent.h"

#include "AbilitySystemComponent.h"
#include "DRCharacterUpgradeProfile.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

UDRCharacterUpgradeComponent::UDRCharacterUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRCharacterUpgradeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDRCharacterUpgradeComponent, Upgrades, COND_OwnerOnly);
}

int32 UDRCharacterUpgradeComponent::GetUpgradeLevel(FGameplayTag UpgradeTag) const
{
	const FDRUpgradeState* State = Upgrades.FindByPredicate(
		[UpgradeTag](const FDRUpgradeState& Entry)
		{
			return Entry.UpgradeTag == UpgradeTag;
		});
	return State ? State->Level : 0;
}

const FDRStatUpgradeData* UDRCharacterUpgradeComponent::GetUpgradeData(FGameplayTag UpgradeTag) const
{
	const UDRCharacterUpgradeProfile* UpgradeProfile = GetProfile();
	return UpgradeProfile->IsUsable() ? UpgradeProfile->FindStatUpgrade(UpgradeTag) : nullptr;
}

const UDRCharacterUpgradeProfile* UDRCharacterUpgradeComponent::GetProfile() const
{
	return IsValid(Profile) ? Profile.Get() : GetDefault<UDRCharacterUpgradeProfile>();
}

bool UDRCharacterUpgradeComponent::TryUpgrade(FGameplayTag UpgradeTag, int32 ExpectedLevel)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystem = IsValid(PlayerState) ? PlayerState->GetAbilitySystemComponent() : nullptr;
	const FDRStatUpgradeData* UpgradeData = GetUpgradeData(UpgradeTag);
	const int32 CurrentLevel = GetUpgradeLevel(UpgradeTag);
	const TCHAR* FailureReason = !IsValid(PlayerState) ? TEXT("MissingPlayerState")
		: !PlayerState->HasAuthority() ? TEXT("NoAuthority")
		: !IsValid(AbilitySystem) ? TEXT("MissingAbilitySystem")
		: IsUpdating ? TEXT("UpdateInProgress")
		: (ExpectedLevel < 0 || ExpectedLevel == MAX_int32) ? TEXT("InvalidExpectedLevel")
		: ExpectedLevel != CurrentLevel ? TEXT("StaleLevel")
		: !UpgradeData ? TEXT("InvalidProfileOrUpgradeTag")
		: !UpgradeData->IsUpgradeAvailable(CurrentLevel) ? TEXT("MaxLevelReached")
		: nullptr;
	if (FailureReason)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Upgrade][ApplyFailed] Reason=%s Player=%s Tag=%s ExpectedLevel=%d CurrentLevel=%d"),
			FailureReason, *GetNameSafe(PlayerState), *UpgradeTag.ToString(), ExpectedLevel, CurrentLevel);
		return false;
	}
	TGuardValue<bool> UpdateGuard(IsUpdating, true);
	TMap<FGameplayTag, float> Values;
	if (!BuildEffectValues(*AbilitySystem, UpgradeTag, ExpectedLevel + 1, Values)
		|| !ApplyUpgradeEffect(*AbilitySystem, UpgradeTag, Values))
	{
		return false;
	}

	FDRUpgradeState* State = Upgrades.FindByPredicate(
		[UpgradeTag](const FDRUpgradeState& Entry)
		{
			return Entry.UpgradeTag == UpgradeTag;
		});
	if (!State)
	{
		State = &Upgrades.AddDefaulted_GetRef();
		State->UpgradeTag = UpgradeTag;
	}
	State->Level = ExpectedLevel + 1;
	PlayerState->ForceNetUpdate();
	OnUpgradesChanged.Broadcast();
	return true;
}


bool UDRCharacterUpgradeComponent::BuildEffectValues(UAbilitySystemComponent& AbilitySystem,
	FGameplayTag UpgradeTag, int32 NextLevel, TMap<FGameplayTag, float>& Values) const
{
	const UDRCharacterUpgradeProfile* UpgradeProfile = GetProfile();

	// 미구매 항목은 원래 스탯을 유지한다.
	const UGameplayEffect* Effect = UpgradeProfile->EffectClass.GetDefaultObject();
	for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
	{
		if (!AbilitySystem.HasAttributeSetForAttribute(Modifier.Attribute))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Upgrade][ApplyFailed] Reason=MissingAttribute Player=%s Tag=%s Attribute=%s"),
				*GetNameSafe(GetOwner()), *UpgradeTag.ToString(), *Modifier.Attribute.GetName());
			return false;
		}
		Values.Add(Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag, 1.f);
	}
	for (const FDRStatUpgradeData& Data : UpgradeProfile->GetStatUpgrades())
	{
		const int32 Level = Data.UpgradeTag == UpgradeTag ? NextLevel : GetUpgradeLevel(Data.UpgradeTag);
		Values.Add(Data.SetByCallerTag, Data.GetStatMultiplier(Level));
	}

	return true;
}

bool UDRCharacterUpgradeComponent::ApplyUpgradeEffect(UAbilitySystemComponent& AbilitySystem,
	FGameplayTag UpgradeTag, const TMap<FGameplayTag, float>& Values)
{
	const UDRCharacterUpgradeProfile* UpgradeProfile = GetProfile();

	if (AbilitySystem.GetActiveGameplayEffect(EffectHandle))
	{
		AbilitySystem.UpdateActiveGameplayEffectSetByCallerMagnitudes(EffectHandle, Values);
	}
	else
	{
		FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(
			UpgradeProfile->EffectClass, 1.f, AbilitySystem.MakeEffectContext());
		if (!Spec.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Upgrade][ApplyFailed] Reason=InvalidEffectSpec Player=%s Tag=%s Effect=%s"),
				*GetNameSafe(GetOwner()), *UpgradeTag.ToString(), *GetNameSafe(UpgradeProfile->EffectClass.Get()));
			return false;
		}
		Spec.Data->AddDynamicAssetTag(DRGameplayTags::Effect_Policy_PersistThroughDeath);
		for (const TPair<FGameplayTag, float>& Value : Values)
		{
			Spec.Data->SetSetByCallerMagnitude(Value.Key, Value.Value);
		}
		EffectHandle = AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!EffectHandle.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Upgrade][ApplyFailed] Reason=EffectApplicationRejected Player=%s Tag=%s Effect=%s"),
				*GetNameSafe(GetOwner()), *UpgradeTag.ToString(), *GetNameSafe(UpgradeProfile->EffectClass.Get()));
			return false;
		}
	}

	return true;
}

void UDRCharacterUpgradeComponent::ResetUpgrades()
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || IsUpdating)
	{
		return;
	}
	TGuardValue<bool> UpdateGuard(IsUpdating, true);
	if (UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent())
	{
		AbilitySystem->RemoveActiveGameplayEffect(EffectHandle);
	}
	EffectHandle.Invalidate();
	Upgrades.Reset();
	PlayerState->ForceNetUpdate();
	OnUpgradesChanged.Broadcast();
}

void UDRCharacterUpgradeComponent::OnRep_Upgrades()
{
	OnUpgradesChanged.Broadcast();
}
