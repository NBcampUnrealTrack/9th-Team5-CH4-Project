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

void UDRShopWidget::InitializeInventoryPanels(
	UDRInventoryComponent* InventoryComponent,
	UDRPerkComponent* PerkComponent)
{
	if (IsValid(BuyPanel))
	{
		BuyPanel->InitializeInventory(InventoryComponent);
	}
}

void UDRShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(BuyPanel))
	{
		BuyPanel->OnOfferRequested.AddDynamic(this, &ThisClass::HandleOfferRequested);
	}

	HandleBuyPanelButtonClicked();
}

void UDRShopWidget::NativeDestruct()
{

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(BuyPanel))
	{
		BuyPanel->OnOfferRequested.RemoveDynamic(this, &ThisClass::HandleOfferRequested);
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

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UDRShopWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}

void UDRShopWidget::HandleSellRequested(
	EDRShopSellTargetType TargetType,
	FGuid InstanceId)
{
	OnSellRequested.Broadcast(TargetType, InstanceId);
}
