#include "DRShopItemWidget.h"

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
	IsWidgetConstructed = true;
	ApplyItemDefinition();
}

void UDRShopItemWidget::NativeDestruct()
{
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
