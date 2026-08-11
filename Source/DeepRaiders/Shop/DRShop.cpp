#include "DRShop.h"

#include "Components/SceneComponent.h"
#include "DeepRaiders/Shop/Components/DRInteractionComponent.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"
#include "DeepRaiders/Shop/Components/DRUpgradeComponent.h"

ADRShop::ADRShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 상점 액터는 상호작용과 UI 기능을 컴포넌트로 구성합니다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InteractionComponent =
		CreateDefaultSubobject<UDRInteractionComponent>(
			TEXT("InteractionComponent"));
	InteractionComponent->SetupAttachment(Root);

	UpgradeComponent = CreateDefaultSubobject<UDRUpgradeComponent>(
		TEXT("UpgradeComponent"));

	ShopUIComponent = CreateDefaultSubobject<UDRShopUIComponent>(
		TEXT("ShopUIComponent"));
}
