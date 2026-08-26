#include "DRShopSellViewModel.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"

void UDRShopSellViewModel::Initialize(
	UDRInventoryComponent* InInventoryComponent,
	UDRPerkComponent* InPerkComponent)
{
	Deinitialize();
	InventoryComponent = InInventoryComponent;
	PerkComponent = InPerkComponent;

	if (InventoryComponent.IsValid())
	{
		InventoryComponent->OnInventoryChangedDelegate.AddUniqueDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}

	if (PerkComponent.IsValid())
	{
		PerkComponent->OnPerksChanged.AddUniqueDynamic(this, &ThisClass::HandlePerksChanged);
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

	if (PerkComponent.IsValid())
	{
		PerkComponent->OnPerksChanged.RemoveDynamic(this, &ThisClass::HandlePerksChanged);
	}

	InventoryComponent.Reset();
	PerkComponent.Reset();
	ClearSelection();
}

void UDRShopSellViewModel::SelectItem(FGuid InInstanceId)
{
	if (!InInstanceId.IsValid())
	{
		ClearSelection();
		return;
	}

	SelectedTargetType = EDRShopSellTargetType::Item;
	SelectedInstanceId = InInstanceId;
	RefreshSelection();
}

void UDRShopSellViewModel::SelectPerk(FGuid InPerkInstanceId)
{
	if (!InPerkInstanceId.IsValid())
	{
		ClearSelection();
		return;
	}

	SelectedTargetType = EDRShopSellTargetType::Perk;
	SelectedInstanceId = InPerkInstanceId;
	RefreshSelection();
}

void UDRShopSellViewModel::HandleInventoryChanged()
{
	RefreshSelection();
}

void UDRShopSellViewModel::HandlePerksChanged()
{
	RefreshSelection();
}

void UDRShopSellViewModel::RefreshSelection()
{
	const bool IsPerkSelected = SelectedTargetType == EDRShopSellTargetType::Perk;
	const FDRItemInstance* ItemInstance = !IsPerkSelected && InventoryComponent.IsValid()
		? InventoryComponent->FindItemInstance(SelectedInstanceId)
		: nullptr;
	const UDRItemDefinition* Definition = IsPerkSelected && PerkComponent.IsValid()
		? static_cast<UDRItemDefinition*>(PerkComponent->FindPerkDefinition(SelectedInstanceId))
		: ItemInstance ? ItemInstance->Definition.Get() : nullptr;

	if (!IsValid(Definition))
	{
		ClearSelection();
		return;
	}

	const int32 SellPrice = Definition->GetSellPrice();
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, Definition->Icon);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Definition->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Definition->Description);
	UE_MVVM_SET_PROPERTY_VALUE(SellPriceText, FText::AsNumber(SellPrice));
	UE_MVVM_SET_PROPERTY_VALUE(bCanSell, Definition->IsSellable());
}

void UDRShopSellViewModel::ClearSelection()
{
	SelectedInstanceId.Invalidate();
	SelectedTargetType = EDRShopSellTargetType::Item;
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Description, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SellPriceText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bCanSell, false);
}
