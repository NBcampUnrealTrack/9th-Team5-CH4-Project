#include "DRShopWeaponUpgradeWidget.h"

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DRShopWeaponUpgradeEntryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRWeaponUpgradeViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRShopWeaponUpgradeWidget::InitializeOffers(const TArray<FDRShopOfferView>& Offers)
{
	ViewModel = NewObject<UDRWeaponUpgradeViewModel>(this);
	ViewModel->Initialize(Offers);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	ensureMsgf(IsValid(View) && View->SetViewModel(TEXT("WeaponUpgradeViewModel"), ViewModel),
		TEXT("WeaponUpgradeViewModel binding is missing on %s"), *GetName());
}

void UDRShopWeaponUpgradeWidget::SetEntries(const TArray<UDRWeaponUpgradeEntryViewModel*>& Entries)
{
	if (!IsValid(UpgradeList) || !EntryWidgetClass)
	{
		return;
	}
	UpgradeList->ClearChildren();
	for (UDRWeaponUpgradeEntryViewModel* Entry : Entries)
	{
		if (!IsValid(Entry))
		{
			continue;
		}
		UDRShopWeaponUpgradeEntryWidget* Widget = CreateWidget<UDRShopWeaponUpgradeEntryWidget>(this, EntryWidgetClass);
		if (!IsValid(Widget))
		{
			continue;
		}
		Widget->InitializeViewModel(Entry);
		Widget->OnOfferRequested.AddDynamic(this, &ThisClass::HandleOfferRequested);
		UVerticalBoxSlot* UpgradeSlot = UpgradeList->AddChildToVerticalBox(Widget);
		UpgradeSlot->SetHorizontalAlignment(HAlign_Fill);
		UpgradeSlot->SetVerticalAlignment(VAlign_Center);
		UpgradeSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
}

void UDRShopWeaponUpgradeWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}
