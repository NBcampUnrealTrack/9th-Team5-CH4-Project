#include "DRShop.h"

#include "Components/SceneComponent.h"
#include "DeepRaiders/Shop/Components/DRInteractionComponent.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"

ADRShop::ADRShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InteractionComponent =
		CreateDefaultSubobject<UDRInteractionComponent>(
			TEXT("InteractionComponent"));
	InteractionComponent->SetupAttachment(Root);

	ShopUIComponent = CreateDefaultSubobject<UDRShopUIComponent>(
		TEXT("ShopUIComponent"));
}
