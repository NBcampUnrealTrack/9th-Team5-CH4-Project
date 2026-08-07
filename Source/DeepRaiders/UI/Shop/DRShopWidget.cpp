#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "DeepRaiders/Shop/Data/DRShopTestData.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::InitializeItems(
	const UDRShopTestData* ShopCatalog)
{
	if (!IsValid(ShopCatalog))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Shop items initialization failed: ShopCatalog is invalid."));
		return;
	}

	if (!ItemWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Shop items initialization failed: ItemWidgetClass is invalid."));
		return;
	}

	if (!IsValid(ItemScrollBox))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Shop items initialization failed: ItemScrollBox is invalid."));
		return;
	}

	ItemScrollBox->ClearChildren();
	int32 CreatedItemCount = 0;

	for (const FDRShopItemData& ItemData : ShopCatalog->Items)
	{
		UDRShopItemWidget* ItemWidget =
			CreateWidget<UDRShopItemWidget>(
				GetOwningPlayer(),
				ItemWidgetClass);

		if (!IsValid(ItemWidget))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Shop item widget creation failed: Item=%s."),
				*ItemData.ItemName.ToString());
			continue;
		}

		ItemWidget->SetItemData(ItemData);
		ItemScrollBox->AddChild(ItemWidget);
		++CreatedItemCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("Shop items created successfully: %d/%d items."),
		CreatedItemCount,
		ShopCatalog->Items.Num());
}

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
