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
