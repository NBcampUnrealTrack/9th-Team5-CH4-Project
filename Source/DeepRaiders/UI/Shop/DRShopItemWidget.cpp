#include "DRShopItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/UI/ViewModel/DRShopViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRShopItemWidget::InitializeViewModel(UDRShopOfferEntryViewModel* NewViewModel)
{
	EntryViewModel = NewViewModel;

	if (!IsValid(EntryViewModel))
	{
		return;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!IsValid(View) || !View->SetViewModel(EntryViewModelName, EntryViewModel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Shop Entry ViewModel '%s' is not registered on %s"),
			*EntryViewModelName.ToString(), *GetName());
	}

	SetOffer(EntryViewModel->GetOffer());
}

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
		|| !IsValid(DescriptionText)
		|| !IsValid(ItemIcon))
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
		|| !IsValid(DescriptionText)
		|| !IsValid(ItemIcon))
	{
		return;
	}

	DisplayNameText->SetText(Offer.DisplayName);
	DescriptionText->SetText(Offer.Description);
	ItemIcon->SetBrushFromTexture(Offer.Icon);
	ItemIcon->SetVisibility(
		IsValid(Offer.Icon)
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	PriceText->SetText(FText::AsNumber(Offer.Price));
	Buy->SetIsEnabled(Offer.IsPurchasable);
}

void UDRShopItemWidget::HandleBuyButtonClicked()
{
	if (!Offer.Request.RowName.IsNone() && Offer.IsPurchasable)
	{
		OnOfferRequested.Broadcast(Offer.Request);
	}
}
