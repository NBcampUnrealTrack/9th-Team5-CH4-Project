#include "DRSnowInteractionComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameFramework/Pawn.h"

UDRSnowInteractionComponent::UDRSnowInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FDRSnowInteractionContext UDRSnowInteractionComponent::MakeInteractionContext() const
{
	FDRSnowInteractionContext Context;
	Context.TeamId = ResolveTeamId();
	Context.SourceActor = GetOwner();

	if (const AActor* Owner = GetOwner())
	{
		Context.InstigatorPawn = Owner->GetInstigator();
	}

	return Context;
}

AActor* UDRSnowInteractionComponent::GetInteractableActorFromHit(
	const FHitResult& HitResult) const
{
	if (AActor* HitActor = HitResult.GetActor())
	{
		return HitActor;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent) ? HitComponent->GetOwner() : nullptr;
}

int32 UDRSnowInteractionComponent::ResolveTeamId() const
{
	if (TeamIdOverride != INDEX_NONE)
	{
		return TeamIdOverride;
	}

	const AActor* Owner = GetOwner();
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!IsValid(OwnerPawn) && IsValid(Owner))
	{
		OwnerPawn = Owner->GetInstigator();
	}

	if (!IsValid(OwnerPawn))
	{
		return INDEX_NONE;
	}

	const ADRPlayerState* DRPlayerState =
		OwnerPawn->GetPlayerState<ADRPlayerState>();
	return IsValid(DRPlayerState)
		? DRPlayerState->GetTeamId()
		: INDEX_NONE;
}
