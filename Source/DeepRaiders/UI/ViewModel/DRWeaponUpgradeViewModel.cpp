#include "DRWeaponUpgradeViewModel.h"

#include "Engine/Texture2D.h"

void UDRWeaponUpgradeEntryViewModel::Initialize(const FDRShopOfferView& NewOffer)
{
	Offer = NewOffer;
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Offer.DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, FText::TrimPrecedingAndTrailing(Offer.Description));
	UE_MVVM_SET_PROPERTY_VALUE(LevelText, FText::Format(
		NSLOCTEXT("Shop", "WeaponUpgradeLevel", "Lv. {0} / {1}"),
		FText::AsNumber(Offer.Request.ExpectedLevel), FText::AsNumber(Offer.MaxLevel)));
	FString Steps;
	for (int32 Level = 0; Level < Offer.MaxLevel; ++Level)
	{
		Steps += Level < Offer.Request.ExpectedLevel ? TEXT("● ") : TEXT("○ ");
	}
	UE_MVVM_SET_PROPERTY_VALUE(LevelStepsText, FText::FromString(Steps.TrimEnd()));
	UE_MVVM_SET_PROPERTY_VALUE(UpgradeButtonText, Offer.Request.ExpectedLevel >= Offer.MaxLevel
		? NSLOCTEXT("Shop", "WeaponUpgradeComplete", "최대 레벨")
		: FText::Format(NSLOCTEXT("Shop", "WeaponUpgradeCost", "강화 · {0}"), FText::AsNumber(Offer.Price)));
	UE_MVVM_SET_PROPERTY_VALUE(IsPurchasable, Offer.IsPurchasable);
}

bool UDRWeaponUpgradeEntryViewModel::GetPurchaseRequest(FDRShopOfferRequest& Request) const
{
	if (!IsPurchasable || !Offer.Request.IsValidRequest())
	{
		return false;
	}
	Request = Offer.Request;
	return true;
}

void UDRWeaponUpgradeViewModel::Initialize(const TArray<FDRShopOfferView>& Offers)
{
	UE_MVVM_SET_PROPERTY_VALUE(WeaponName, Offers.IsEmpty() ? FText::GetEmpty() : Offers[0].WeaponName);
	FSlateBrush Brush;
	UTexture2D* Icon = Offers.IsEmpty() ? nullptr : Offers[0].WeaponIcon.Get();
	if (IsValid(Icon))
	{
		Brush.SetResourceObject(Icon);
		Brush.ImageSize = FVector2D(Icon->GetSizeX(), Icon->GetSizeY());
	}
	else
	{
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
	}
	UE_MVVM_SET_PROPERTY_VALUE(WeaponBrush, Brush);
	TArray<TObjectPtr<UDRWeaponUpgradeEntryViewModel>> NewEntries;
	for (const FDRShopOfferView& Offer : Offers)
	{
		if (!Offer.Request.UpgradeTag.IsValid())
		{
			continue;
		}
		UDRWeaponUpgradeEntryViewModel* Entry = NewObject<UDRWeaponUpgradeEntryViewModel>(this);
		Entry->Initialize(Offer);
		NewEntries.Add(Entry);
	}
	UE_MVVM_SET_PROPERTY_VALUE(Entries, MoveTemp(NewEntries));
}
