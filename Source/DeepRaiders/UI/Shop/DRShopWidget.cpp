#include "DRShopWidget.h"

#include "Components/Button.h"

void UDRShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}
}

void UDRShopWidget::NativeDestruct()
{
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}

	Super::NativeDestruct();
}

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}
