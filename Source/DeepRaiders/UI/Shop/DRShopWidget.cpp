#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::InitializeItems(
	const TArray<TObjectPtr<UDRItemDefinition>>& NewItemDefinitions)
{
	ItemDefinitions = NewItemDefinitions;
	RefreshItems(EItemCategory::Equipment);
}

void UDRShopWidget::RefreshItems(EItemCategory Category)
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
		if (!IsValid(ItemDefinition)
			|| ItemDefinition->Category != Category)
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
		ItemWidget->OnPurchaseRequested.AddDynamic(
			this,
			&ThisClass::HandlePurchaseRequested);
		ItemScrollBox->AddChild(ItemWidget);
		++CreatedItemCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("Shop category items created successfully: %d items."),
		CreatedItemCount);
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

	EquipmentButton->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleEquipmentButtonClicked);
	ConsumableButton->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleConsumableButtonClicked);
}

void UDRShopWidget::NativeDestruct()
{
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}

	EquipmentButton->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleEquipmentButtonClicked);
	ConsumableButton->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleConsumableButtonClicked);

	Super::NativeDestruct();
}

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UDRShopWidget::HandleEquipmentButtonClicked()
{
	RefreshItems(EItemCategory::Equipment);
}

void UDRShopWidget::HandleConsumableButtonClicked()
{
	RefreshItems(EItemCategory::Consumable);
}

void UDRShopWidget::HandlePurchaseRequested(
	UDRItemDefinition* ItemDefinition)
{
	OnPurchaseRequested.Broadcast(ItemDefinition);
}
