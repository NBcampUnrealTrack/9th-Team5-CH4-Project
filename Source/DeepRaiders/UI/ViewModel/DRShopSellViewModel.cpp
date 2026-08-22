#include "DRShopSellViewModel.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"

void UDRShopSellViewModel::Initialize(UDRInventoryComponent* InInventoryComponent)
{
	Deinitialize();
	InventoryComponent = InInventoryComponent;

	if (InventoryComponent.IsValid())
	{
		InventoryComponent->OnInventoryChangedDelegate.AddUniqueDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}
}

void UDRShopSellViewModel::Deinitialize()
{
	if (InventoryComponent.IsValid())
	{
		InventoryComponent->OnInventoryChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}

	InventoryComponent.Reset();
	ClearSelection();
}

void UDRShopSellViewModel::SelectItem(FGuid InInstanceId)
{
	SelectedInstanceId = InInstanceId;
	RefreshSelection();
}

void UDRShopSellViewModel::HandleInventoryChanged()
{
	RefreshSelection();
}

void UDRShopSellViewModel::RefreshSelection()
{
	const FDRItemInstance* ItemInstance = InventoryComponent.IsValid()
		? InventoryComponent->FindItemInstance(SelectedInstanceId)
		: nullptr;
	const UDRItemDefinition* Definition = ItemInstance
		? ItemInstance->Definition
		: nullptr;

	if (!IsValid(Definition))
	{
		ClearSelection();
		return;
	}

	const int32 SellPrice = Definition->Price / 2;
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, Definition->Icon);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Definition->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Definition->Description);
	UE_MVVM_SET_PROPERTY_VALUE(SellPriceText, FText::AsNumber(SellPrice));
	UE_MVVM_SET_PROPERTY_VALUE(bCanSell, Definition->bCanBeSold && SellPrice > 0);
}

void UDRShopSellViewModel::ClearSelection()
{
	SelectedInstanceId.Invalidate();
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Description, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SellPriceText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bCanSell, false);
}
