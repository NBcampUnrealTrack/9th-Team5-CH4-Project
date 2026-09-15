#include "DRAbilitySystemComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayCueInterface.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

void UDRAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	AActor* PreviousAvatarActor = GetAvatarActor();
	const bool bEngineWillReplayActiveEffectCues =
		(PreviousAvatarActor == nullptr || GetAvatarActor_Direct() == nullptr) && InAvatarActor != nullptr;

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (!IsValid(InAvatarActor) || PreviousAvatarActor == InAvatarActor)
	{
		return;
	}

	// InitAbilityActorInfo는 Avatar가 없던 경우 ActiveGameplayEffects의 Cue만 복원한다.
	ReplayGameplayCueContainer(ActiveGameplayCues);

	if (ShouldUseActiveGameplayEffectsForCueReplay())
	{
		if (!bEngineWillReplayActiveEffectCues)
		{
			ReplayActiveGameplayEffectCues();
		}
	}
	else
	{
		ReplayGameplayCueContainer(MinimalReplicationGameplayCues);
	}
}

void UDRAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	GameplayEffectRemovedDelegateHandle = OnAnyGameplayEffectRemovedDelegate().AddUObject(
		this, &ThisClass::HandleGameplayEffectRemoved);
}

void UDRAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GameplayEffectRemovedDelegateHandle.IsValid())
	{
		OnAnyGameplayEffectRemovedDelegate().Remove(GameplayEffectRemovedDelegateHandle);
		GameplayEffectRemovedDelegateHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UDRAbilitySystemComponent::HandleGameplayEffectRemoved(const FActiveGameplayEffect& RemovedEffect)
{
	if (!IsOwnerActorAuthoritative() || ReplicationMode == EGameplayEffectReplicationMode::Full
		|| RemovedEffect.bIsInhibited || !IsValid(RemovedEffect.Spec.Def))
	{
		return;
	}

	bool bRestoredAnyCue = false;
	TSet<FGameplayTag> ProcessedCueTags;

	for (const FGameplayEffectCue& RemovedCue : RemovedEffect.Spec.Def->GameplayCues)
	{
		for (const FGameplayTag& GameplayCueTag : RemovedCue.GameplayCueTags)
		{
			if (!GameplayCueTag.MatchesTag(DRGameplayTags::GameplayCue_VFX)
				|| ProcessedCueTags.Contains(GameplayCueTag)
				|| MinimalReplicationGameplayCues.HasCue(GameplayCueTag))
			{
				continue;
			}

			ProcessedCueTags.Add(GameplayCueTag);

			const FActiveGameplayEffect* RemainingEffect = FindRemainingEffectWithCue(GameplayCueTag);
			if (!RemainingEffect)
			{
				continue;
			}

			// 보존이 필요한 경우 다시 부착한다.
			FGameplayCueParameters CueParameters(RemainingEffect->Spec.GetEffectContext());
			MinimalReplicationGameplayCues.AddCue(GameplayCueTag, FPredictionKey(), CueParameters);
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, CueParameters);
			bRestoredAnyCue = true;
		}
	}

	if (bRestoredAnyCue)
	{
		ForceReplication();
	}
}

const FActiveGameplayEffect* UDRAbilitySystemComponent::FindRemainingEffectWithCue(
	const FGameplayTag& GameplayCueTag) const
{
	for (const FActiveGameplayEffect& ActiveEffect : &ActiveGameplayEffects)
	{
		if (ActiveEffect.IsPendingRemove || ActiveEffect.bIsInhibited || !IsValid(ActiveEffect.Spec.Def))
		{
			continue;
		}

		for (const FGameplayEffectCue& ActiveCue : ActiveEffect.Spec.Def->GameplayCues)
		{
			if (ActiveCue.GameplayCueTags.HasTagExact(GameplayCueTag))
			{
				return &ActiveEffect;
			}
		}
	}

	return nullptr;
}

void UDRAbilitySystemComponent::ReplayGameplayCueContainer(const FActiveGameplayCueContainer& GameplayCueContainer)
{
	for (const FActiveGameplayCue& ActiveCue : GameplayCueContainer.GameplayCues)
	{
		if (!ActiveCue.bPredictivelyRemoved && ActiveCue.GameplayCueTag.IsValid())
		{
			InvokeGameplayCueEvent(
				ActiveCue.GameplayCueTag, EGameplayCueEvent::WhileActive, ActiveCue.Parameters);
		}
	}
}

void UDRAbilitySystemComponent::ReplayActiveGameplayEffectCues()
{
	for (const FActiveGameplayEffect& ActiveEffect : &ActiveGameplayEffects)
	{
		if (ActiveEffect.IsPendingRemove || ActiveEffect.bIsInhibited || !IsValid(ActiveEffect.Spec.Def)
			|| ActiveEffect.Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			continue;
		}

		InvokeGameplayCueEvent(ActiveEffect.Spec, EGameplayCueEvent::WhileActive);
	}
}

bool UDRAbilitySystemComponent::ShouldUseActiveGameplayEffectsForCueReplay() const
{
	return IsOwnerActorAuthoritative() || ReplicationMode == EGameplayEffectReplicationMode::Full
		|| (AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled());
}
