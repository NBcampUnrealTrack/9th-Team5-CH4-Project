#include "DRGA_SearchSkill.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRSilhouetteComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"

void UDRGA_SearchSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	if (!IsValid(Character)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if ((IsVFXVisibleToAll && ActorInfo->IsNetAuthority())
		|| (!IsVFXVisibleToAll && ActorInfo->IsLocallyControlled()))
	{
		PlaySearchVFX(Character);
	}

	if (ActorInfo->IsLocallyControlled())
	{
		LocalRevealId = FGuid::NewGuid();
		RevealEnemies(Character, false);
	}

	if (ActorInfo->IsNetAuthority() && HasTeamSharePerk(ActorInfo))
	{
		SharedRevealId = FGuid::NewGuid();
		RevealEnemies(Character, true);
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, RevealDuration);
	if (!IsValid(WaitTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitTask->OnFinish.AddDynamic(this, &ThisClass::HandleSearchFinished);
	WaitTask->ReadyForActivation();
}

void UDRGA_SearchSkill::PlaySearchVFX(ADRPlayerCharacter* Character) const
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(Character)
		|| !IsValid(AbilitySystem))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Character->GetActorLocation();
	Parameters.RawMagnitude = SearchRadius;
	Parameters.Instigator = Character;
	Parameters.EffectCauser = Character;
	Parameters.SourceObject = GetCurrentSkillDefinition();

	AbilitySystem->ExecuteGameplayCue(
		DRGameplayTags::GameplayCue_VFX_Skill_Search,
		Parameters);
}

void UDRGA_SearchSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool IsReplicateEndAbility,
	bool IsWasCancelled)
{
	if (ActorInfo != nullptr && ActorInfo->IsLocallyControlled())
	{
		StopReveals(false);
	}

	if (ActorInfo != nullptr && ActorInfo->IsNetAuthority())
	{
		StopReveals(true);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, IsReplicateEndAbility, IsWasCancelled);
}

void UDRGA_SearchSkill::HandleSearchFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UDRGA_SearchSkill::RevealEnemies(
	const ADRPlayerCharacter* Character,
	bool IsSharedReveal)
{
	UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	const int32 SourceTeamId = DRCombatTeam::GetActorTeamId(Character);
	if (!IsValid(World)
		|| SourceTeamId == INDEX_NONE)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SearchSkill), false, Character);
	const FCollisionObjectQueryParams ObjectQueryParams(ECC_Pawn);
	World->OverlapMultiByObjectType(
		OverlapResults,
		Character->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SearchRadius),
		QueryParams);

	TSet<ADRPlayerCharacter*> RevealedActors;
	const ADRPlayerState* SourcePlayerState = Character->GetPlayerState<ADRPlayerState>();
	const int32 SourcePlayerId = IsValid(SourcePlayerState)
		? SourcePlayerState->GetPlayerId()
		: INDEX_NONE;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(OverlapResult.GetActor());
		const int32 TargetTeamId = DRCombatTeam::GetActorTeamId(TargetCharacter);
		if (!IsValid(TargetCharacter)
			|| TargetTeamId == INDEX_NONE
			|| TargetTeamId == SourceTeamId
			|| RevealedActors.Contains(TargetCharacter))
		{
			continue;
		}

		RevealedActors.Add(TargetCharacter);
		if (IsSharedReveal)
		{
			TargetCharacter->MulticastStartSharedSearchReveal(
				SourceTeamId,
				SourcePlayerId,
				SharedRevealId,
				RevealDuration,
				StencilValue);
			SharedRevealedCharacters.Add(TargetCharacter);
		}
		else if (UDRSilhouetteComponent* SilhouetteComponent =
			TargetCharacter->GetSilhouetteComponent())
		{
			SilhouetteComponent->StartSearchReveal(
				LocalRevealId,
				RevealDuration,
				StencilValue);
			LocalRevealedCharacters.Add(TargetCharacter);
		}
	}
}

void UDRGA_SearchSkill::StopReveals(bool IsSharedReveal)
{
	TArray<TWeakObjectPtr<ADRPlayerCharacter>>& RevealedCharacters = IsSharedReveal
		? SharedRevealedCharacters
		: LocalRevealedCharacters;
	const FGuid RevealId = IsSharedReveal ? SharedRevealId : LocalRevealId;
	for (const TWeakObjectPtr<ADRPlayerCharacter>& RevealedCharacter : RevealedCharacters)
	{
		ADRPlayerCharacter* Character = RevealedCharacter.Get();
		if (!IsValid(Character))
		{
			continue;
		}

		if (IsSharedReveal)
		{
			Character->MulticastStopSharedSearchReveal(RevealId);
		}
		else if (UDRSilhouetteComponent* SilhouetteComponent =
			Character->GetSilhouetteComponent())
		{
			SilhouetteComponent->StopSearchReveal(RevealId);
		}
	}

	RevealedCharacters.Reset();
	if (IsSharedReveal)
	{
		SharedRevealId.Invalidate();
	}
	else
	{
		LocalRevealId.Invalidate();
	}
}

bool UDRGA_SearchSkill::HasTeamSharePerk(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	return IsValid(PerkComponent)
		&& IsValid(SkillDefinition)
		&& PerkComponent->HasSkillPerk(
			SkillDefinition->SkillId,
			DRGameplayTags::Perk_Skill_Search_TeamShare);
}
