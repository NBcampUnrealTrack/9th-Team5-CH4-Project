#include "DRShop.h"

#include "Components/SceneComponent.h"
#include "DeepRaiders/Shop/Components/DRShopAreaComponent.h"
#include "DeepRaiders/Shop/Components/DRShopComponent.h"
#include "DeepRaiders/Shop/Components/DRUpgradeComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "GameFramework/Pawn.h"
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

	static ConstructorHelpers::FObjectFinder<USoundBase> PurchaseSoundAsset(
		TEXT("/Game/DeepRaiders/Sound/SoundWave/Shop_Buy.Shop_Buy"));

	PurchaseSound = PurchaseSoundAsset.Object;
}

void ADRShop::BeginPlay()
{
	Super::BeginPlay();

	ShopAreaComponent->OnPawnEntered.AddDynamic(
		this,
		&ThisClass::HandlePawnEntered);
	ShopAreaComponent->OnPawnExited.AddDynamic(
		this,
		&ThisClass::HandlePawnExited);
}

void ADRShop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ShopAreaComponent))
	{
		ShopAreaComponent->OnPawnEntered.RemoveDynamic(
			this,
			&ThisClass::HandlePawnEntered);
		ShopAreaComponent->OnPawnExited.RemoveDynamic(
			this,
			&ThisClass::HandlePawnExited);
	}

	Super::EndPlay(EndPlayReason);
}

void ADRShop::HandlePawnEntered(APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Pawn->GetController()))
	{
		PlayerController->SetAvailableShop(this);
	}
}

void ADRShop::HandlePawnExited(APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Pawn->GetController()))
	{
		PlayerController->ClearAvailableShop(this);
	}
}
