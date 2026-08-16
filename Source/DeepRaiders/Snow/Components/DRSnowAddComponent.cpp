#include "DRSnowAddComponent.h"

#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"

bool UDRSnowAddComponent::TryAddSnowFromHit(
	const FHitResult& HitResult)
{
	if (!HitResult.bBlockingHit)
	{
		return false;
	}

	const FDRSnowSurfaceAddRequest Request =
		MakeAddRequest(HitResult.ImpactPoint, HitResult.ImpactNormal);

	bool bHandled = false;
	AActor* TargetActor = GetInteractableActorFromHit(HitResult);
	if (IsValid(TargetActor) &&
		TargetActor->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowAdd(TargetActor, Request))
	{
		IDRSnowInteractableInterface::Execute_ReceiveSnowAdded(TargetActor, Request);
		bHandled = true;
	}

	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

bool UDRSnowAddComponent::TryAddSnowAtLocation(
	FVector WorldLocation,
	FVector SurfaceNormal)
{
	const FDRSnowSurfaceAddRequest Request =
		MakeAddRequest(WorldLocation, SurfaceNormal);

	const bool bHandled = Request.Amount > 0.f && Request.Radius > 0.f;
	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

FDRSnowSurfaceAddRequest UDRSnowAddComponent::MakeAddRequest(
	FVector WorldLocation,
	FVector SurfaceNormal) const
{
	FDRSnowSurfaceAddRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal =
		SurfaceNormal.IsNearlyZero()
			? FVector::UpVector
			: SurfaceNormal.GetSafeNormal();
	Request.Radius = AddRadius;
	Request.Amount = AddAmount;
	Request.Context = MakeInteractionContext();
	return Request;
}
