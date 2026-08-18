#include "DRShopItemWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UDRShopItemWidget::SetOffer(
	const FDRShopOfferView& NewOffer)
{
	Offer = NewOffer;

	if (IsWidgetConstructed)
	{
		ApplyOffer();
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
	ApplyOffer();
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

void UDRShopItemWidget::ApplyOffer()
{
	if (!IsValid(Buy)
		|| !IsValid(DisplayNameText)
		|| !IsValid(PriceText)
		|| !IsValid(DescriptionText))
	{
		return;
	}

	DisplayNameText->SetText(Offer.DisplayName);
	DescriptionText->SetText(Offer.Description);
	PriceText->SetText(FText::AsNumber(Offer.Price));
	Buy->SetIsEnabled(Offer.IsPurchasable);

	if (UTextBlock* ButtonText = Cast<UTextBlock>(Buy->GetContent()))
	{
		const bool IsUpgrade =
			Offer.Request.OfferType == EDRShopOfferType::Upgrade;
		ButtonText->SetText(FText::FromString(
			IsUpgrade ? TEXT("업그레이드") : TEXT("구매")));
	}
}

void UDRShopItemWidget::HandleBuyButtonClicked()
{
	if (!Offer.Request.RowName.IsNone() && Offer.IsPurchasable)
	{
		OnOfferRequested.Broadcast(Offer.Request);
	}
}
