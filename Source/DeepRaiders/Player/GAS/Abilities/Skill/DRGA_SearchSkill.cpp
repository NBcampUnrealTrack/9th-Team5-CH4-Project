#include "DRGA_SearchSkill.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

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

	if (ActorInfo->IsLocallyControlled())
	{
		RevealEnemies(Character);
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

void UDRGA_SearchSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool IsReplicateEndAbility,
	bool IsWasCancelled)
{
	RestoreSilhouettes();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, IsReplicateEndAbility, IsWasCancelled);
}

void UDRGA_SearchSkill::HandleSearchFinished()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UDRGA_SearchSkill::RevealEnemies(const ADRPlayerCharacter* Character)
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

	TSet<AActor*> RevealedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		const int32 TargetTeamId = DRCombatTeam::GetActorTeamId(TargetActor);
		if (!IsValid(TargetActor)
			|| TargetTeamId == INDEX_NONE
			|| TargetTeamId == SourceTeamId
			|| RevealedActors.Contains(TargetActor))
		{
			continue;
		}

		RevealedActors.Add(TargetActor);
		RevealActor(TargetActor);
	}
}

void UDRGA_SearchSkill::RevealActor(AActor* Actor)
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		FDRSearchSilhouetteState& State = SilhouetteStates.AddDefaulted_GetRef();
		State.Component = PrimitiveComponent;
		State.IsRenderCustomDepthEnabled = PrimitiveComponent->bRenderCustomDepth;
		State.StencilValue = PrimitiveComponent->CustomDepthStencilValue;

		PrimitiveComponent->SetCustomDepthStencilValue(StencilValue);
		PrimitiveComponent->SetRenderCustomDepth(true);
	}
}

void UDRGA_SearchSkill::RestoreSilhouettes()
{
	for (const FDRSearchSilhouetteState& State : SilhouetteStates)
	{
		UPrimitiveComponent* PrimitiveComponent = State.Component.Get();
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->SetCustomDepthStencilValue(State.StencilValue);
		PrimitiveComponent->SetRenderCustomDepth(State.IsRenderCustomDepthEnabled);
	}

	SilhouetteStates.Reset();
}
