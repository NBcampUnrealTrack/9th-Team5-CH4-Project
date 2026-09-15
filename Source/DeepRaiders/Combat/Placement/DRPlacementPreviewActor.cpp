#include "DRPlacementPreviewActor.h"

#include "Components/SceneComponent.h"

ADRPlacementPreviewActor::ADRPlacementPreviewActor()
{
	SetReplicates(false);
	SetActorEnableCollision(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADRPlacementPreviewActor::UpdatePreview(const FVector& Location, const FVector& SurfaceNormal,
	const FVector& AimDirection, bool bCanPlace, const FVector& Dimensions)
{
	SetActorLocation(Location);
	SetActorHiddenInGame(false);
	OnPreviewUpdated(SurfaceNormal, AimDirection, bCanPlace, Dimensions);
}

void ADRPlacementPreviewActor::HidePreview()
{
	SetActorHiddenInGame(true);
}
