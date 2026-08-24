#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
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
