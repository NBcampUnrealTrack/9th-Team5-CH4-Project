#include "DRPerkComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

UDRPerkComponent::UDRPerkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRPerkComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UDRPerkComponent,
		PerkStates,
		COND_OwnerOnly);
}

int32 UDRPerkComponent::GetPerkRank(FName RowName) const
{
	const FDRPerkState* PerkState = PerkStates.FindByPredicate(
		[RowName](const FDRPerkState& State)
		{
			return State.RowName == RowName;
		});

	return PerkState ? PerkState->Rank : 0;
}

bool UDRPerkComponent::CanApplyNextRank(
	FName RowName,
	const FDRPerkTableRow& PerkRow) const
{
	const int32 TargetRank = GetPerkRank(RowName) + 1;
	const UGameplayEffect* Effect = PerkRow.EffectClass.GetDefaultObject();

	return !RowName.IsNone()
		&& PerkRow.IsValidRank(TargetRank)
		&& IsValid(Effect)
		&& Effect->DurationPolicy == EGameplayEffectDurationType::Infinite;
}

bool UDRPerkComponent::ApplyNextRank(
	FName RowName,
	const FDRPerkTableRow& PerkRow)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;
	const int32 TargetRank = GetPerkRank(RowName) + 1;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !CanApplyNextRank(RowName, PerkRow))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][ApplyFailed] Player=%s Row=%s TargetRank=%d Reason=InvalidStateOrDefinition"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			TargetRank);
		return false;
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle EffectSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			PerkRow.EffectClass,
			static_cast<float>(TargetRank),
			EffectContext);

	if (!EffectSpec.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][ApplyFailed] Player=%s Row=%s TargetRank=%d Effect=%s Reason=InvalidSpec"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			TargetRank,
			*GetNameSafe(PerkRow.EffectClass));
		return false;
	}

	const FActiveGameplayEffectHandle NewHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
			*EffectSpec.Data.Get());

	if (!NewHandle.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][ApplyFailed] Player=%s Row=%s TargetRank=%d Effect=%s Reason=InvalidActiveEffectHandle"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			TargetRank,
			*GetNameSafe(PerkRow.EffectClass));
		return false;
	}

	if (const FActiveGameplayEffectHandle* PreviousHandle =
		EffectHandles.Find(RowName))
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(*PreviousHandle);
	}

	EffectHandles.Add(RowName, NewHandle);

	FDRPerkState* PerkState = FindPerkState(RowName);

	if (!PerkState)
	{
		PerkState = &PerkStates.AddDefaulted_GetRef();
		PerkState->RowName = RowName;
	}

	PerkState->Rank = TargetRank;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][EffectApplied] Player=%s Row=%s Name=%s Rank=%d Effect=%s HandleValid=%d"),
		*GetNameSafe(PlayerState),
		*RowName.ToString(),
		*PerkRow.DisplayName.ToString(),
		TargetRank,
		*GetNameSafe(PerkRow.EffectClass),
		NewHandle.IsValid());
	OnPerksChanged.Broadcast();
	return true;
}

void UDRPerkComponent::OnRep_PerkStates()
{
	OnPerksChanged.Broadcast();
}

FDRPerkState* UDRPerkComponent::FindPerkState(FName RowName)
{
	return PerkStates.FindByPredicate(
		[RowName](const FDRPerkState& State)
		{
			return State.RowName == RowName;
		});
}
