#include "DRHUDViewModel.h"

#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"

void UDRHUDViewModel::HandleQuickSlotsChanged()
{
	RefreshAmmoVisibility();
}

void UDRHUDViewModel::HandleSelectedQuickSlotItemChanged(UDRItemDefinition*)
{
	RefreshAmmoVisibility();
}

void UDRHUDViewModel::RefreshAmmoVisibility()
{
	FDRItemInstance SelectedItem;
	const int32 SelectedSlotIndex = QuickSlotComponent.IsValid()
		? QuickSlotComponent->GetSelectedSlotIndex()
		: INDEX_NONE;
	const bool bHasSelectedItem = QuickSlotComponent.IsValid()
		&& QuickSlotComponent->GetQuickSlot(SelectedSlotIndex, SelectedItem);
	const UDRRangedWeaponDefinition* RangedWeapon = bHasSelectedItem
		? Cast<UDRRangedWeaponDefinition>(SelectedItem.Definition)
		: nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(bIsAmmoVisible, IsValid(RangedWeapon));
}
