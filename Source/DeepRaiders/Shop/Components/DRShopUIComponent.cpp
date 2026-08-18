#include "DRShopUIComponent.h"

#include "DRShopAreaComponent.h"
#include "DRShopComponent.h"
#include "DRShopTransactionComponent.h"
#include "DRUpgradeComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRShopUIComponent::BeginPlay()
{
	Super::BeginPlay();

	ShopAreaComponent =
		GetOwner()->FindComponentByClass<UDRShopAreaComponent>();
	ShopComponent = GetOwner()->FindComponentByClass<UDRShopComponent>();
	UpgradeComponent = GetOwner()->FindComponentByClass<UDRUpgradeComponent>();

	if (!IsValid(ShopAreaComponent)
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent))
	{
		return;
	}

	// 상점 범위 진입과 이탈에 맞춰 상호작용 가능 상태를 변경한다.
	ShopAreaComponent->OnPawnEntered.AddDynamic(
		this,
		&ThisClass::HandlePawnEntered);
	ShopAreaComponent->OnPawnExited.AddDynamic(
		this,
		&ThisClass::HandlePawnExited);
}

void UDRShopUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
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

	HideShopWidget();
	Super::EndPlay(EndPlayReason);
}

void UDRShopUIComponent::HandlePawnEntered(APawn* Pawn)
{
	if (!IsValid(Pawn)
		|| !Pawn->IsLocallyControlled())
	{
		return;
	}

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Pawn->GetController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	// 입력을 처리할 로컬 플레이어에게 현재 상점을 등록한다.
	PlayerController->SetAvailableShop(this);
}

void UDRShopUIComponent::ToggleShopWidget()
{
	if (IsValid(ShopWidget))
	{
		HideShopWidget();
		return;
	}

	ShowShopWidget();
}

void UDRShopUIComponent::ShowShopWidget()
{
	if (IsValid(ShopWidget)
		|| !ShopWidgetClass
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent))
	{
		return;
	}

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(GetWorld()->GetFirstPlayerController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	ShopTransactionComponent =
		PlayerController->GetShopTransactionComponent();
	InventoryComponent = PlayerController->GetInventoryComponent();

	if (!IsValid(ShopTransactionComponent)
		|| !IsValid(InventoryComponent))
	{
		return;
	}

	ShopWidget = CreateWidget<UDRShopWidget>(
		PlayerController,
		ShopWidgetClass);

	if (!IsValid(ShopWidget))
	{
		return;
	}

	// 위젯에 상점 데이터를 전달하고 UI 요청 이벤트를 연결한다.
	ShopWidget->InitializeShop(ShopComponent->GetItemOffers());
	RefreshUpgradeOffers();
	ShopWidget->OnCloseRequested.AddDynamic(
		this,
		&ThisClass::HideShopWidget);
	ShopWidget->OnOfferRequested.AddDynamic(
		this,
		&ThisClass::HandleOfferRequested);
	ShopWidget->OnSellAllOresRequested.AddDynamic(
		this,
		&ThisClass::HandleSellAllOresRequested);
	InventoryComponent->OnInventoryChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleInventoryChanged);
	ShopWidget->AddToViewport();

	// 상점 UI를 조작할 수 있도록 마우스와 입력 모드를 전환한다.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->FlushPressedKeys();
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;
}

void UDRShopUIComponent::HandlePawnExited(APawn* Pawn)
{
	if (IsValid(Pawn) && Pawn->IsLocallyControlled())
	{
		if (ADRPlayerController* PlayerController =
			Cast<ADRPlayerController>(Pawn->GetController()))
		{
			PlayerController->ClearAvailableShop(this);
		}

		// 범위를 벗어나면 열려 있는 상점 UI도 함께 닫는다.
		HideShopWidget();
	}
}

void UDRShopUIComponent::HideShopWidget()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}

	APlayerController* PlayerController = IsValid(ShopWidget)
		? ShopWidget->GetOwningPlayer()
		: nullptr;

	if (IsValid(ShopWidget))
	{
		ShopWidget->OnCloseRequested.RemoveDynamic(
			this,
			&ThisClass::HideShopWidget);
		ShopWidget->OnOfferRequested.RemoveDynamic(
			this,
			&ThisClass::HandleOfferRequested);
		ShopWidget->OnSellAllOresRequested.RemoveDynamic(
			this,
			&ThisClass::HandleSellAllOresRequested);
		ShopWidget->RemoveFromParent();
	}

	ShopWidget = nullptr;
	InventoryComponent = nullptr;
	ShopTransactionComponent = nullptr;

	if (IsValid(PlayerController))
	{
		// 상점 종료 후 게임 입력 상태로 복구한다.
		PlayerController->FlushPressedKeys();
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}

void UDRShopUIComponent::HandleOfferRequested(FDRShopOfferRequest Request)
{
	if (IsValid(ShopTransactionComponent))
	{
		ShopTransactionComponent->RequestOffer(GetOwner(), Request);
	}
}

void UDRShopUIComponent::HandleSellAllOresRequested()
{
	if (IsValid(ShopTransactionComponent))
	{
		ShopTransactionComponent->RequestSellAllOres(GetOwner());
	}
}

void UDRShopUIComponent::HandleInventoryChanged()
{
	RefreshUpgradeOffers();
}

void UDRShopUIComponent::RefreshUpgradeOffers()
{
	if (!IsValid(ShopWidget)
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent)
		|| !IsValid(InventoryComponent))
	{
		return;
	}

	ShopWidget->SetUpgradeOffers(
		UpgradeComponent->GetNextUpgradeOffers(
			ShopComponent,
			InventoryComponent));
}
