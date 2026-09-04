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
	PriceText->SetText(Offer.Request.OfferType == EDRShopOfferType::WeaponUpgrade && !Offer.Request.UpgradeTag.IsValid()
		? FText::GetEmpty() : FText::AsNumber(Offer.Price));
	Buy->SetIsEnabled(Offer.IsPurchasable);
}

void UDRShopItemWidget::HandleBuyButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Shop][BuyClicked] Widget=%s Type=%d Row=%s Tag=%s ExpectedLevel=%d"),
		*GetName(), static_cast<int32>(Offer.Request.OfferType), *Offer.Request.RowName.ToString(),
		*Offer.Request.UpgradeTag.ToString(), Offer.Request.ExpectedLevel);
	if (!Offer.IsPurchasable || !Offer.Request.IsValidRequest() || !OnOfferRequested.IsBound())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=UI Reason=%s Tag=%s"),
			!Offer.IsPurchasable ? TEXT("NotPurchasable")
				: (!Offer.Request.IsValidRequest() ? TEXT("InvalidRequest") : TEXT("MissingRequestBinding")),
			*Offer.Request.UpgradeTag.ToString());
		return;
	}
	OnOfferRequested.Broadcast(Offer.Request);
}
