#include "DRSnowDamageComponent.h"

#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"

bool UDRSnowDamageComponent::TryApplySnowDamageFromHit(
	const FHitResult& HitResult)
{
	if (!HitResult.bBlockingHit)
	{
		return false;
	}

	return TryApplySnowDamageToActor(
		GetInteractableActorFromHit(HitResult),
		HitResult.ImpactPoint,
		HitResult.ImpactNormal);
}

bool UDRSnowDamageComponent::TryApplySnowDamageToActor(
	AActor* TargetActor,
	FVector HitLocation,
	FVector HitNormal)
{
	const FDRSnowDamageRequest Request =
		MakeDamageRequest(HitLocation, HitNormal);

	bool bHandled = false;
	if (IsValid(TargetActor) &&
		TargetActor->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowDamage(TargetActor, Request))
	{
		IDRSnowInteractableInterface::Execute_ReceiveSnowDamage(TargetActor, Request);
		bHandled = true;
	}

	OnSnowDamaged.Broadcast(Request, bHandled);
	return bHandled;
}

FDRSnowDamageRequest UDRSnowDamageComponent::MakeDamageRequest(
	FVector HitLocation,
	FVector HitNormal) const
{
	FDRSnowDamageRequest Request;
	Request.HitLocation = HitLocation;
	Request.HitNormal =
		HitNormal.IsNearlyZero()
			? FVector::UpVector
			: HitNormal.GetSafeNormal();
	Request.DamageAmount = DamageAmount;
	Request.Context = MakeInteractionContext();
	return Request;
}
