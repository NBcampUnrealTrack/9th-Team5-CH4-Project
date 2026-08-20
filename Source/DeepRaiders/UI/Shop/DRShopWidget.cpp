#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::SetOffers(
	EDRShopOfferType OfferType,
	const TArray<FDRShopOfferView>& NewOffers)
{
	Offers.RemoveAll(
		[OfferType](const FDRShopOfferView& Offer)
		{
			return Offer.Request.OfferType == OfferType;
		});
	Offers.Append(NewOffers);
	RefreshSelectedSection();
}

void UDRShopWidget::SelectSection(EDRShopOfferSection Section)
{
	if (!IsValid(EquipmentButton)
		|| !IsValid(ConsumableButton)
		|| !IsValid(UpgradeButton)
		|| !IsValid(PerkButton))
	{
		return;
	}

	SelectedSection = Section;
	EquipmentButton->SetIsEnabled(Section != EDRShopOfferSection::Equipment);
	ConsumableButton->SetIsEnabled(Section != EDRShopOfferSection::Consumable);
	UpgradeButton->SetIsEnabled(Section != EDRShopOfferSection::Upgrade);
	PerkButton->SetIsEnabled(Section != EDRShopOfferSection::Perk);
	RefreshSelectedSection();
}

void UDRShopWidget::RefreshSelectedSection()
{
	if (!IsValid(ItemScrollBox) || !ItemWidgetClass)
	{
		return;
	}

	ItemScrollBox->ClearChildren();

	for (const FDRShopOfferView& Offer : Offers)
	{
		if (Offer.Section != SelectedSection)
		{
			continue;
		}

		CreateItemWidget(Offer);
	}
}

void UDRShopWidget::CreateItemWidget(const FDRShopOfferView& Offer)
{
	UDRShopItemWidget* ItemWidget = CreateWidget<UDRShopItemWidget>(
		GetOwningPlayer(),
		ItemWidgetClass);

	if (!IsValid(ItemWidget))
	{
		return;
	}

	ItemWidget->SetOffer(Offer);
	ItemWidget->OnOfferRequested.AddDynamic(
		this,
		&ThisClass::HandleOfferRequested);
	ItemScrollBox->AddChild(ItemWidget);
}

void UDRShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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


	SelectSection(SelectedSection);
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

	Super::NativeDestruct();
}

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UDRShopWidget::HandleEquipmentButtonClicked()
{
	SelectSection(EDRShopOfferSection::Equipment);
}

void UDRShopWidget::HandleConsumableButtonClicked()
{
	SelectSection(EDRShopOfferSection::Consumable);
}

void UDRShopWidget::HandleUpgradeButtonClicked()
{
	SelectSection(EDRShopOfferSection::Upgrade);
}

void UDRShopWidget::HandlePerkButtonClicked()
{
	SelectSection(EDRShopOfferSection::Perk);
}

void UDRShopWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}
