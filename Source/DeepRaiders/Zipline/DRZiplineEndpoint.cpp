#include "DRZiplineEndpoint.h"

#include "Components/SceneComponent.h"

ADRZiplineEndpoint::ADRZiplineEndpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SetRootComponent(Root);
}
