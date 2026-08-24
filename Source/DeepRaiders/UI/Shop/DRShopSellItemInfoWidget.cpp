#include "DRShopSellItemInfoWidget.h"

#include "Components/Button.h"
#include "DeepRaiders/UI/ViewModel/DRShopSellViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRShopSellItemInfoWidget::InitializeViewModel(UDRShopSellViewModel* NewViewModel)
{
	SellViewModel = NewViewModel;
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!IsValid(SellViewModel) || !IsValid(View)
		|| !View->SetViewModel(SellViewModelName, SellViewModel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Sell ViewModel '%s' is not registered on %s"),
			*SellViewModelName.ToString(), *GetName());
	}
}

void UDRShopSellItemInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(SellButton))
	{
		SellButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSellButtonClicked);
	}
}

void UDRShopSellItemInfoWidget::NativeDestruct()
{
	if (IsValid(SellButton))
	{
		SellButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSellButtonClicked);
	}

	SellViewModel = nullptr;
	Super::NativeDestruct();
}

void UDRShopSellItemInfoWidget::HandleSellButtonClicked()
{
	if (IsValid(SellViewModel) && SellViewModel->CanSell())
	{
		OnSellRequested.Broadcast(SellViewModel->GetSelectedInstanceId());
	}
}
