#include "DRShopItemWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UDRShopItemWidget::SetItemData(
	const FDRShopItemData& NewItemData)
{
	ItemData = NewItemData;
	IsItemDataSet = true;

	if (IsWidgetConstructed)
	{
		ApplyItemData();
	}
}

void UDRShopItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	IsWidgetConstructed = true;
	ApplyItemData();
}

void UDRShopItemWidget::NativeDestruct()
{
	IsWidgetConstructed = false;
	Super::NativeDestruct();
}

void UDRShopItemWidget::ApplyItemData()
{
	if (!IsItemDataSet)
	{
		return;
	}

	ItemIcon->SetBrushFromTexture(ItemData.Icon);
	ItemNameText->SetText(ItemData.ItemName);
	PriceText->SetText(FText::AsNumber(ItemData.Price));
	InformationText->SetText(ItemData.Information);
}
