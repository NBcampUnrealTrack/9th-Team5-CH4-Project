#include "DRShopWeaponUpgradeEntryWidget.h"

#include "Components/Button.h"
#include "DeepRaiders/UI/ViewModel/DRWeaponUpgradeViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRShopWeaponUpgradeEntryWidget::InitializeViewModel(UDRWeaponUpgradeEntryViewModel* NewViewModel)
{
	ViewModel = NewViewModel;
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	ensureMsgf(IsValid(View) && View->SetViewModel(TEXT("WeaponUpgradeEntryViewModel"), ViewModel),
		TEXT("WeaponUpgradeEntryViewModel binding is missing on %s"), *GetName());
}

void UDRShopWeaponUpgradeEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (IsValid(UpgradeButton))
	{
		UpgradeButton->OnClicked.AddDynamic(this, &ThisClass::HandleUpgradeClicked);
	}
}

void UDRShopWeaponUpgradeEntryWidget::HandleUpgradeClicked()
{
	FDRShopOfferRequest Request;
	if (IsValid(ViewModel) && ViewModel->GetPurchaseRequest(Request))
	{
		OnOfferRequested.Broadcast(Request);
	}
}
