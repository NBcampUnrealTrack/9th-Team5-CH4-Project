#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::InitializeItems(
	const TArray<TObjectPtr<UDRItemDefinition>>& ItemDefinitions)
{
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

	for (UDRItemDefinition* ItemDefinition : ItemDefinitions)
	{
		if (!IsValid(ItemDefinition))
		{
			continue;
		}

		UDRShopItemWidget* ItemWidget =
			CreateWidget<UDRShopItemWidget>(
				GetOwningPlayer(),
				ItemWidgetClass);

		if (!IsValid(ItemWidget))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Shop item widget creation failed: Item=%s."),
				*ItemDefinition->DisplayName.ToString());
			continue;
		}

		ItemWidget->SetItemDefinition(ItemDefinition);
		ItemScrollBox->AddChild(ItemWidget);
		++CreatedItemCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("Shop items created successfully: %d/%d items."),
		CreatedItemCount,
		ItemDefinitions.Num());
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
