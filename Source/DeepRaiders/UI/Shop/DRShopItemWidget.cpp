#include "DRShopItemWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

void UDRShopItemWidget::SetItemOffer(
	const FDRShopItemOffer& NewItemOffer)
{
	ItemOffer = NewItemOffer;

	if (IsWidgetConstructed)
	{
		ApplyItemDefinition();
	}
}

void UDRShopItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(Buy)
		|| !IsValid(DisplayNameText)
		|| !IsValid(PriceText)
		|| !IsValid(DescriptionText))
	{
		return;
	}

	Buy->OnClicked.AddDynamic(this, &ThisClass::HandleBuyButtonClicked);
	IsWidgetConstructed = true;
	ApplyItemDefinition();
}

void UDRShopItemWidget::NativeDestruct()
{
	if (IsValid(Buy))
	{
		Buy->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleBuyButtonClicked);
	}

	IsWidgetConstructed = false;
	Super::NativeDestruct();
}

void UDRShopItemWidget::ApplyItemDefinition()
{
	UDRItemDefinition* ItemDefinition = ItemOffer.ItemDefinition;

	if (!IsValid(ItemDefinition)
		|| !IsValid(Buy)
		|| !IsValid(DisplayNameText)
		|| !IsValid(PriceText)
		|| !IsValid(DescriptionText))
	{
		return;
	}

	if (ItemOffer.IsUpgrade())
	{
		if (IsValid(ItemOffer.UpgradeSourceDefinition))
		{
			DisplayNameText->SetText(FText::Format(
				FText::FromString(TEXT("{0} → {1}")),
				ItemOffer.UpgradeSourceDefinition->DisplayName,
				ItemDefinition->DisplayName));
		}
		else
		{
			DisplayNameText->SetText(ItemDefinition->DisplayName);
		}

		if (UTextBlock* ButtonText = Cast<UTextBlock>(Buy->GetContent()))
		{
			ButtonText->SetText(FText::FromString(TEXT("업그레이드")));
		}
	}
	else
	{
		DisplayNameText->SetText(ItemDefinition->DisplayName);
	}

	DescriptionText->SetText(ItemDefinition->Description);
	PriceText->SetText(FText::AsNumber(ItemDefinition->Price));
}

void UDRShopItemWidget::HandleBuyButtonClicked()
{
	if (!ItemOffer.RowName.IsNone()
		&& IsValid(ItemOffer.ItemDefinition))
	{
		OnOfferRequested.Broadcast(ItemOffer.MakeRequest());
	}
}
