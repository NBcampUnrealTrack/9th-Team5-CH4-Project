#include "DRShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRPerkResetTestWidget.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::InitializeShop(
	const TArray<FDRShopOfferView>& NewItemOffers)
{
	ItemOffers = NewItemOffers;
	SelectCategory(EDRItemCategory::Equipment);
}

void UDRShopWidget::SetUpgradeOffers(
	const TArray<FDRShopOfferView>& NewUpgradeOffers)
{
	UpgradeOffers = NewUpgradeOffers;

	if (IsUpgradeSelected)
	{
		RefreshUpgradeItems();
	}
}

void UDRShopWidget::SetPerkOffers(
	const TArray<FDRShopOfferView>& NewPerkOffers)
{
	PerkOffers = NewPerkOffers;

	if (IsPerkSelected)
	{
		RefreshPerkItems();
	}
}

void UDRShopWidget::SelectCategory(EDRItemCategory Category)
{
	if (!IsValid(EquipmentButton) || !IsValid(ConsumableButton))
	{
		return;
	}

	IsUpgradeSelected = false;
	IsPerkSelected = false;
	EquipmentButton->SetIsEnabled(Category != EDRItemCategory::Equipment);
	ConsumableButton->SetIsEnabled(Category != EDRItemCategory::Consumable);

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->SetIsEnabled(true);
	}

	if (IsValid(PerkButton))
	{
		PerkButton->SetIsEnabled(true);
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

	const EDRShopOfferSection Section = Category == EDRItemCategory::Equipment
		? EDRShopOfferSection::Equipment
		: EDRShopOfferSection::Consumable;

	for (const FDRShopOfferView& Offer : ItemOffers)
	{
		if (Offer.Section != Section)
		{
			continue;
		}

		CreateItemWidget(Offer);
	}
}

void UDRShopWidget::RefreshUpgradeItems()
{
	if (!IsValid(ItemScrollBox) || !ItemWidgetClass)
	{
		return;
	}

	ItemScrollBox->ClearChildren();

	for (const FDRShopOfferView& Offer : UpgradeOffers)
	{
		CreateItemWidget(Offer);
	}
}

void UDRShopWidget::RefreshPerkItems()
{
	if (!IsValid(ItemScrollBox) || !ItemWidgetClass)
	{
		return;
	}

	ItemScrollBox->ClearChildren();
	CreatePerkResetTestWidget();

	for (const FDRShopOfferView& Offer : PerkOffers)
	{
		CreateItemWidget(Offer);
	}
}

bool UDRShopWidget::CreatePerkResetTestWidget()
{
	if (!PerkResetTestWidgetClass)
	{
		return false;
	}

	UDRPerkResetTestWidget* ResetWidget =
		CreateWidget<UDRPerkResetTestWidget>(
			GetOwningPlayer(),
			PerkResetTestWidgetClass);

	if (!IsValid(ResetWidget))
	{
		return false;
	}

	ItemScrollBox->AddChild(ResetWidget);
	return true;
}

bool UDRShopWidget::CreateItemWidget(const FDRShopOfferView& Offer)
{
	UDRShopItemWidget* ItemWidget = CreateWidget<UDRShopItemWidget>(
		GetOwningPlayer(),
		ItemWidgetClass);

	if (!IsValid(ItemWidget))
	{
		return false;
	}

	ItemWidget->SetOffer(Offer);
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
	InitializePerkButton();

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

	if (IsValid(PerkButton))
	{
		PerkButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandlePerkButtonClicked);
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

void UDRShopWidget::InitializePerkButton()
{
	if (IsValid(PerkButton)
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

	PerkButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("PerkButton"));
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>();

	if (!IsValid(PerkButton) || !IsValid(ButtonText))
	{
		PerkButton = nullptr;
		return;
	}

	ButtonText->SetText(FText::FromString(TEXT("\uD37D")));
	PerkButton->SetContent(ButtonText);
	ButtonContainer->AddChild(PerkButton);
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

	if (IsValid(PerkButton))
	{
		PerkButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandlePerkButtonClicked);
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
	IsPerkSelected = false;
	EquipmentButton->SetIsEnabled(true);
	ConsumableButton->SetIsEnabled(true);
	UpgradeButton->SetIsEnabled(false);

	if (IsValid(PerkButton))
	{
		PerkButton->SetIsEnabled(true);
	}

	RefreshUpgradeItems();
}

void UDRShopWidget::HandlePerkButtonClicked()
{
	if (!IsValid(EquipmentButton)
		|| !IsValid(ConsumableButton)
		|| !IsValid(PerkButton))
	{
		return;
	}

	IsUpgradeSelected = false;
	IsPerkSelected = true;
	EquipmentButton->SetIsEnabled(true);
	ConsumableButton->SetIsEnabled(true);
	PerkButton->SetIsEnabled(false);

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->SetIsEnabled(true);
	}

	RefreshPerkItems();
}

void UDRShopWidget::HandleSellAllOresButtonClicked()
{
	OnSellAllOresRequested.Broadcast();
}

void UDRShopWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}
