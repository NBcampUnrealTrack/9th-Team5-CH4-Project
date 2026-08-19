#include "DRShop.h"

#include "Components/SceneComponent.h"
#include "DeepRaiders/Shop/Components/DRShopAreaComponent.h"
#include "DeepRaiders/Shop/Components/DRShopComponent.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"
#include "DeepRaiders/Shop/Components/DRUpgradeComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ADRShop::ADRShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 상점 액터는 접근 범위와 UI 기능을 컴포넌트로 구성합니다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ShopAreaComponent = CreateDefaultSubobject<UDRShopAreaComponent>(
		TEXT("ShopAreaComponent"));
	ShopAreaComponent->SetupAttachment(Root);

	ShopComponent = CreateDefaultSubobject<UDRShopComponent>(
		TEXT("ShopComponent"));

	UpgradeComponent = CreateDefaultSubobject<UDRUpgradeComponent>(
		TEXT("UpgradeComponent"));

	ShopUIComponent = CreateDefaultSubobject<UDRShopUIComponent>(
		TEXT("ShopUIComponent"));

	static ConstructorHelpers::FObjectFinder<USoundBase> PurchaseSoundAsset(
		TEXT("/Game/DeepRaiders/Sound/SoundWave/Shop_Buy.Shop_Buy"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SellSoundAsset(
		TEXT("/Game/DeepRaiders/Sound/SoundWave/Shop_Sell.Shop_Sell"));

	PurchaseSound = PurchaseSoundAsset.Object;
	SellSound = SellSoundAsset.Object;
}
