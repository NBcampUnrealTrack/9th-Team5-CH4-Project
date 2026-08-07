#include "DRShopItemWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

void UDRShopItemWidget::SetItemDefinition(
	UDRItemDefinition* NewItemDefinition)
{
	ItemDefinition = NewItemDefinition;

	if (IsWidgetConstructed)
	{
		ApplyItemDefinition();
	}
}

void UDRShopItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Buy->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleBuyButtonClicked);
	IsWidgetConstructed = true;
	ApplyItemDefinition();
}

void UDRShopItemWidget::NativeDestruct()
{
	Buy->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleBuyButtonClicked);
	IsWidgetConstructed = false;
	Super::NativeDestruct();
}

void UDRShopItemWidget::ApplyItemDefinition()
{
	if (!IsValid(ItemDefinition))
	{
		return;
	}

	DisplayNameText->SetText(ItemDefinition->DisplayName);
	DescriptionText->SetText(ItemDefinition->Description);
	PriceText->SetText(FText::AsNumber(ItemDefinition->Price));
}

void UDRShopItemWidget::HandleBuyButtonClicked()
{
	if (IsValid(ItemDefinition))
	{
		OnPurchaseRequested.Broadcast(ItemDefinition);
	}
}
