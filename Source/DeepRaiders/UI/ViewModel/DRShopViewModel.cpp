#include "DRShopViewModel.h"

void UDRShopOfferEntryViewModel::Initialize(const FDRShopOfferView& InOffer)
{
	Offer = InOffer;
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InOffer.DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, InOffer.Description);
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, InOffer.Icon);
	UE_MVVM_SET_PROPERTY_VALUE(Price, InOffer.Price);
	UE_MVVM_SET_PROPERTY_VALUE(PriceText,
		InOffer.Request.OfferType == EDRShopOfferType::WeaponUpgrade && !InOffer.Request.UpgradeTag.IsValid()
			? FText::GetEmpty() : FText::AsNumber(InOffer.Price));
	UE_MVVM_SET_PROPERTY_VALUE(bIsPurchasable, InOffer.IsPurchasable);
}

void UDRShopViewModel::SetOffers(
	EDRShopOfferType OfferType,
	const TArray<FDRShopOfferView>& NewOffers)
{
	Offers.RemoveAll(
		[OfferType](const FDRShopOfferView& Offer)
		{
			return Offer.Request.OfferType == OfferType;
		});
	Offers.Append(NewOffers);
	RebuildOfferEntries();
}

TArray<UDRShopOfferEntryViewModel*> UDRShopViewModel::GetOfferEntries() const
{
	TArray<UDRShopOfferEntryViewModel*> Entries;
	Entries.Reserve(OfferEntries.Num());

	for (UDRShopOfferEntryViewModel* Entry : OfferEntries)
	{
		Entries.Add(Entry);
	}

	return Entries;
}

void UDRShopViewModel::SelectSection(EDRShopOfferSection NewSection)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedSection, NewSection);
	RebuildOfferEntries();
}

void UDRShopViewModel::Deinitialize()
{
	Offers.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(
		OfferEntries,
		TArray<TObjectPtr<UDRShopOfferEntryViewModel>>());
}

void UDRShopViewModel::RebuildOfferEntries()
{
	TArray<TObjectPtr<UDRShopOfferEntryViewModel>> NewEntries;

	for (const FDRShopOfferView& Offer : Offers)
	{
		if (Offer.Section != SelectedSection)
		{
			continue;
		}

		UDRShopOfferEntryViewModel* Entry = NewObject<UDRShopOfferEntryViewModel>(this);
		Entry->Initialize(Offer);
		NewEntries.Add(Entry);
	}

	UE_MVVM_SET_PROPERTY_VALUE(OfferEntries, MoveTemp(NewEntries));
}
