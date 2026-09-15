#include "DRStartingWeaponViewModel.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRStartingWeaponTable.h"
#include "DeepRaiders/Player/Components/DRStartingSelectionComponent.h"
#include "Engine/DataTable.h"

void UDRStartingWeaponEntryViewModel::Select()
{
	if (OwnerViewModel.IsValid())
	{
		OwnerViewModel->SelectWeapon(this);
	}
}

void UDRStartingWeaponEntryViewModel::Initialize(
	UDRStartingWeaponViewModel* InOwnerViewModel,
	FName InRowName,
	UDRItemDefinition* InWeaponDefinition,
	const FText& InDescription)
{
	OwnerViewModel = InOwnerViewModel;
	RowName = InRowName;
	UE_MVVM_SET_PROPERTY_VALUE(Description, InDescription);

	if (!IsValid(InWeaponDefinition))
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InWeaponDefinition->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, InWeaponDefinition->Icon);
}

void UDRStartingWeaponEntryViewModel::SetSelected(bool IsNewSelected)
{
	UE_MVVM_SET_PROPERTY_VALUE(IsSelected, IsNewSelected);
}

void UDRStartingWeaponViewModel::Initialize(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	Deinitialize();
	SelectionComponent = InSelectionComponent;
	UDataTable* WeaponTable = IsValid(InSelectionComponent)
		? InSelectionComponent->GetWeaponTable()
		: nullptr;

	TArray<TObjectPtr<UDRStartingWeaponEntryViewModel>> NewEntries;

	if (IsValid(WeaponTable))
	{
		for (const FName RowName : WeaponTable->GetRowNames())
		{
			const FDRStartingWeaponTableRow* Row =
				WeaponTable->FindRow<FDRStartingWeaponTableRow>(RowName, TEXT("StartingWeaponViewModel"));
			UDRItemDefinition* WeaponDefinition = Row
				? Row->WeaponDefinition.LoadSynchronous()
				: nullptr;

			if (!IsValid(WeaponDefinition))
			{
				continue;
			}

			UDRStartingWeaponEntryViewModel* Entry =
				NewObject<UDRStartingWeaponEntryViewModel>(this);
			Entry->Initialize(this, RowName, WeaponDefinition, Row->Description);
			NewEntries.Add(Entry);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(WeaponEntries, MoveTemp(NewEntries));
}

void UDRStartingWeaponViewModel::Deinitialize()
{
	SelectionComponent.Reset();
	SelectedWeapon = nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(IsConfirmEnabled, false);
	UE_MVVM_SET_PROPERTY_VALUE(
		WeaponEntries,
		TArray<TObjectPtr<UDRStartingWeaponEntryViewModel>>());
}

void UDRStartingWeaponViewModel::ConfirmSelection()
{
	if (SelectionComponent.IsValid() && IsValid(SelectedWeapon))
	{
		SelectionComponent->RequestWeaponSelection(SelectedWeapon->RowName);
	}
}

void UDRStartingWeaponViewModel::SelectWeapon(
	UDRStartingWeaponEntryViewModel* WeaponEntry)
{
	if (!IsValid(WeaponEntry) || !WeaponEntries.Contains(WeaponEntry))
	{
		return;
	}

	if (IsValid(SelectedWeapon))
	{
		SelectedWeapon->SetSelected(false);
	}

	WeaponEntry->SetSelected(true);
	SelectedWeapon = WeaponEntry;
	UE_MVVM_SET_PROPERTY_VALUE(IsConfirmEnabled, true);
}
