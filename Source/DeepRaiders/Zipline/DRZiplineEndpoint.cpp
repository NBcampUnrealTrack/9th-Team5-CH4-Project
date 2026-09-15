#include "DRZiplineEndpoint.h"

#include "Components/SceneComponent.h"

ADRZiplineEndpoint::ADRZiplineEndpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SetRootComponent(Root);

	RopeAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RopeAnchor"));

	RopeAnchor->SetupAttachment(Root);
}

FVector ADRZiplineEndpoint::GetAnchorLocation() const
{
	return IsValid(RopeAnchor) ? RopeAnchor->GetComponentLocation() : GetActorLocation();
}
