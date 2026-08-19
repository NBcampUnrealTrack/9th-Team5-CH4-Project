#include "DRShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::InitializeShop(
	const TArray<FDRShopItemOffer>& NewItemOffers)
{
	ItemOffers = NewItemOffers;
	SelectCategory(EDRItemCategory::Equipment);
}

void UDRShopWidget::SetUpgradeOffers(
	const TArray<FDRShopItemOffer>& NewUpgradeOffers)
{
	UpgradeOffers = NewUpgradeOffers;

	if (IsUpgradeSelected)
	{
		RefreshUpgradeItems();
	}
}

void UDRShopWidget::SelectCategory(EDRItemCategory Category)
{
	if (!IsValid(EquipmentButton) || !IsValid(ConsumableButton))
	{
		return;
	}

	IsUpgradeSelected = false;
	EquipmentButton->SetIsEnabled(Category != EDRItemCategory::Equipment);
	ConsumableButton->SetIsEnabled(Category != EDRItemCategory::Consumable);

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->SetIsEnabled(true);
	}

	RefreshItems(Category);
}

void UDRShopWidget::RefreshItems(EDRItemCategory Category)
{
	if (!IsValid(ItemScrollBox) || !ItemWidgetClass)
	{
		return;
	}

	ItemScrollBox->ClearChildren();

	for (const FDRShopItemOffer& ItemOffer : ItemOffers)
	{
		if (ItemOffer.IsUpgrade()
			|| !IsValid(ItemOffer.ItemDefinition)
			|| ItemOffer.ItemDefinition->Category != Category)
		{
			continue;
		}

		CreateItemWidget(ItemOffer);
	}
}

void UDRShopWidget::RefreshUpgradeItems()
{
	if (!IsValid(ItemScrollBox) || !ItemWidgetClass)
	{
		return;
	}

	ItemScrollBox->ClearChildren();

	for (const FDRShopItemOffer& ItemOffer : UpgradeOffers)
	{
		CreateItemWidget(ItemOffer);
	}
}

bool UDRShopWidget::CreateItemWidget(const FDRShopItemOffer& ItemOffer)
{
	if (!IsValid(ItemOffer.ItemDefinition))
	{
		return false;
	}

	UDRShopItemWidget* ItemWidget = CreateWidget<UDRShopItemWidget>(
		GetOwningPlayer(),
		ItemWidgetClass);

	if (!IsValid(ItemWidget))
	{
		return false;
	}

	ItemWidget->SetItemOffer(ItemOffer);
	ItemWidget->OnOfferRequested.AddDynamic(
		this,
		&ThisClass::HandleOfferRequested);
	ItemScrollBox->AddChild(ItemWidget);
	return true;
}

void UDRShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	InitializeSellAllOresButton();
	InitializeUpgradeButton();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(EquipmentButton))
	{
		EquipmentButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleEquipmentButtonClicked);
	}

	if (IsValid(ConsumableButton))
	{
		ConsumableButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleConsumableButtonClicked);
	}

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleUpgradeButtonClicked);
	}

	if (IsValid(SellAllOresButton))
	{
		SellAllOresButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleSellAllOresButtonClicked);
	}
}

void UDRShopWidget::InitializeSellAllOresButton()
{
	if (IsValid(SellAllOresButton)
		|| !IsValid(ConsumableButton)
		|| !IsValid(WidgetTree))
	{
		return;
	}

	UPanelWidget* ButtonContainer =
		Cast<UPanelWidget>(ConsumableButton->GetParent());

	if (!IsValid(ButtonContainer))
	{
		return;
	}

	SellAllOresButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("SellAllOresButton"));
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>();

	if (!IsValid(SellAllOresButton) || !IsValid(ButtonText))
	{
		SellAllOresButton = nullptr;
		return;
	}

	ButtonText->SetText(FText::FromString(TEXT("광석 전체 판매")));
	SellAllOresButton->SetContent(ButtonText);
	ButtonContainer->AddChild(SellAllOresButton);
}

void UDRShopWidget::InitializeUpgradeButton()
{
	if (IsValid(UpgradeButton)
		|| !IsValid(ConsumableButton)
		|| !IsValid(WidgetTree))
	{
		return;
	}

	UPanelWidget* ButtonContainer =
		Cast<UPanelWidget>(ConsumableButton->GetParent());

	if (!IsValid(ButtonContainer))
	{
		return;
	}

	UpgradeButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("UpgradeButton"));
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>();

	if (!IsValid(UpgradeButton) || !IsValid(ButtonText))
	{
		UpgradeButton = nullptr;
		return;
	}

	ButtonText->SetText(FText::FromString(TEXT("업그레이드")));
	UpgradeButton->SetContent(ButtonText);
	ButtonContainer->AddChild(UpgradeButton);
}

void UDRShopWidget::NativeDestruct()
{
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(EquipmentButton))
	{
		EquipmentButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleEquipmentButtonClicked);
	}

	if (IsValid(ConsumableButton))
	{
		ConsumableButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleConsumableButtonClicked);
	}

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleUpgradeButtonClicked);
	}

	if (IsValid(SellAllOresButton))
	{
		SellAllOresButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleSellAllOresButtonClicked);
	}

	Super::NativeDestruct();
}

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UDRShopWidget::HandleEquipmentButtonClicked()
{
	SelectCategory(EDRItemCategory::Equipment);
}

void UDRShopWidget::HandleConsumableButtonClicked()
{
	SelectCategory(EDRItemCategory::Consumable);
}

void UDRShopWidget::HandleUpgradeButtonClicked()
{
	if (!IsValid(EquipmentButton)
		|| !IsValid(ConsumableButton)
		|| !IsValid(UpgradeButton))
	{
		return;
	}

	IsUpgradeSelected = true;
	EquipmentButton->SetIsEnabled(true);
	ConsumableButton->SetIsEnabled(true);
	UpgradeButton->SetIsEnabled(false);
	RefreshUpgradeItems();
}

void UDRShopWidget::HandleSellAllOresButtonClicked()
{
	OnSellAllOresRequested.Broadcast();
}

void UDRShopWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}
