#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/StartingWeapon/DRStartingWeaponSelectWidget.h"
#include "DRShopBuyPanelWidget.h"
#include "DRShopSellPanelWidget.h"

void UDRShopWidget::SetOffers(
	EDRShopOfferType OfferType,
	const TArray<FDRShopOfferView>& NewOffers)
{
	if (IsValid(BuyPanel))
	{
		BuyPanel->SetOffers(OfferType, NewOffers);
	}
}

void UDRShopWidget::InitializeSellPanel(UDRInventoryComponent* InventoryComponent)
{
	if (IsValid(SellPanel))
	{
		SellPanel->InitializeInventory(InventoryComponent);
	}
}

void UDRShopWidget::InitializeStartingWeaponPanel(
	ADRPlayerController* PlayerController,
	UDataTable* StartingWeaponTable,
	bool IsSelectionAvailable)
{
	StartingWeaponPanelButton->SetVisibility(
		IsSelectionAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (!IsSelectionAvailable)
	{
		StartingWeaponPanel->DeinitializeSelection();
		HandleBuyPanelButtonClicked();
		return;
	}

	StartingWeaponPanel->InitializeSelection(PlayerController, StartingWeaponTable);
	HandleStartingWeaponPanelButtonClicked();
}

void UDRShopWidget::DisableStartingWeaponPanel()
{
	StartingWeaponPanel->DeinitializeSelection();
	StartingWeaponPanelButton->SetVisibility(ESlateVisibility::Collapsed);
	HandleBuyPanelButtonClicked();
}

void UDRShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(BuyPanelButton))
	{
		BuyPanelButton->OnClicked.AddDynamic(this, &ThisClass::HandleBuyPanelButtonClicked);
	}

	if (IsValid(SellPanelButton))
	{
		SellPanelButton->OnClicked.AddDynamic(this, &ThisClass::HandleSellPanelButtonClicked);
	}

	StartingWeaponPanelButton->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleStartingWeaponPanelButtonClicked);

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(BuyPanel))
	{
		BuyPanel->OnOfferRequested.AddDynamic(this, &ThisClass::HandleOfferRequested);
	}

	if (IsValid(SellPanel))
	{
		SellPanel->OnSellRequested.AddDynamic(this, &ThisClass::HandleSellRequested);
	}

	HandleBuyPanelButtonClicked();
}

void UDRShopWidget::NativeDestruct()
{
	if (IsValid(BuyPanelButton))
	{
		BuyPanelButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBuyPanelButtonClicked);
	}

	if (IsValid(SellPanelButton))
	{
		SellPanelButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSellPanelButtonClicked);
	}

	StartingWeaponPanelButton->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleStartingWeaponPanelButtonClicked);

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(BuyPanel))
	{
		BuyPanel->OnOfferRequested.RemoveDynamic(this, &ThisClass::HandleOfferRequested);
	}

	if (IsValid(SellPanel))
	{
		SellPanel->OnSellRequested.RemoveDynamic(this, &ThisClass::HandleSellRequested);
	}

	Super::NativeDestruct();
}

void UDRShopWidget::HandleBuyPanelButtonClicked()
{
	if (IsValid(PanelSwitcher) && IsValid(BuyPanel))
	{
		PanelSwitcher->SetActiveWidget(BuyPanel);
	}
}

void UDRShopWidget::HandleSellPanelButtonClicked()
{
	if (IsValid(PanelSwitcher) && IsValid(SellPanel))
	{
		PanelSwitcher->SetActiveWidget(SellPanel);
	}
}

void UDRShopWidget::HandleStartingWeaponPanelButtonClicked()
{
	if (IsValid(PanelSwitcher) && IsValid(StartingWeaponPanel))
	{
		PanelSwitcher->SetActiveWidget(StartingWeaponPanel);
	}
}

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UDRShopWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}

void UDRShopWidget::HandleSellRequested(FGuid InstanceId)
{
	OnSellRequested.Broadcast(InstanceId);
}
