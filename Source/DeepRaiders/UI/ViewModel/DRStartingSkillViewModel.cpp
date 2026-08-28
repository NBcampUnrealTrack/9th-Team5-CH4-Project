#include "DRStartingSkillViewModel.h"

#include "DeepRaiders/Player/Components/DRStartingSelectionComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/DRStartingSkillTable.h"
#include "Engine/DataTable.h"

void UDRStartingSkillEntryViewModel::Select()
{
	if (OwnerViewModel.IsValid())
	{
		OwnerViewModel->SelectSkill(this);
	}
}

void UDRStartingSkillEntryViewModel::Initialize(
	UDRStartingSkillViewModel* InOwnerViewModel,
	FName InRowName,
	UDRSkillDefinition* InSkillDefinition)
{
	OwnerViewModel = InOwnerViewModel;
	RowName = InRowName;

	if (!IsValid(InSkillDefinition))
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InSkillDefinition->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, InSkillDefinition->Description);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, InSkillDefinition->Icon);
}

void UDRStartingSkillEntryViewModel::SetSelected(bool IsNewSelected)
{
	UE_MVVM_SET_PROPERTY_VALUE(IsSelected, IsNewSelected);
}

void UDRStartingSkillViewModel::Initialize(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	Deinitialize();
	SelectionComponent = InSelectionComponent;

	if (SelectionComponent.IsValid())
	{
		SelectionComponent->OnSelectionAvailabilityChanged.AddUObject(
			this,
			&ThisClass::HandleSelectionStateChanged);
	}

	RefreshSelection();
}

void UDRStartingSkillViewModel::RefreshSelection()
{
	SelectedSkill = nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(IsConfirmEnabled, false);

	UDataTable* SkillTable = SelectionComponent.IsValid()
		? SelectionComponent->GetSkillTable()
		: nullptr;
	const EDRSkillSlot PendingSkillSlot = SelectionComponent.IsValid()
		? SelectionComponent->GetPendingSkillSlot()
		: EDRSkillSlot::Count;
	TArray<TObjectPtr<UDRStartingSkillEntryViewModel>> NewEntries;

	if (IsValid(SkillTable))
	{
		for (const FName RowName : SkillTable->GetRowNames())
		{
			const FDRStartingSkillTableRow* Row =
				SkillTable->FindRow<FDRStartingSkillTableRow>(RowName, TEXT("StartingSkillViewModel"));
			UDRSkillDefinition* SkillDefinition = Row
				? Row->SkillDefinition.LoadSynchronous()
				: nullptr;

			if (!IsValid(SkillDefinition)
				|| SkillDefinition->SkillSlot != PendingSkillSlot)
			{
				continue;
			}

			UDRStartingSkillEntryViewModel* Entry =
				NewObject<UDRStartingSkillEntryViewModel>(this);
			Entry->Initialize(this, RowName, SkillDefinition);
			NewEntries.Add(Entry);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(SkillEntries, MoveTemp(NewEntries));

	const FText NewSelectionGuideText = PendingSkillSlot == EDRSkillSlot::One
		? NSLOCTEXT("StartingSkill", "SelectSkillOne", "스킬 1 선택 (Shift)")
		: NSLOCTEXT("StartingSkill", "SelectSkillTwo", "스킬 2 선택 (C)");
	UE_MVVM_SET_PROPERTY_VALUE(SelectionGuideText, NewSelectionGuideText);
	OnSelectionGuideTextChanged.Broadcast(SelectionGuideText);
}

void UDRStartingSkillViewModel::HandleSelectionStateChanged(bool)
{
	RefreshSelection();
}

TArray<UDRStartingSkillEntryViewModel*> UDRStartingSkillViewModel::GetSkillEntries() const
{
	TArray<UDRStartingSkillEntryViewModel*> Entries;
	Entries.Reserve(SkillEntries.Num());

	for (UDRStartingSkillEntryViewModel* Entry : SkillEntries)
	{
		Entries.Add(Entry);
	}

	return Entries;
}

void UDRStartingSkillViewModel::ConfirmSelection()
{
	if (SelectionComponent.IsValid() && IsValid(SelectedSkill))
	{
		SelectionComponent->RequestSkillSelection(SelectedSkill->RowName);
	}
}

void UDRStartingSkillViewModel::Deinitialize()
{
	if (SelectionComponent.IsValid())
	{
		SelectionComponent->OnSelectionAvailabilityChanged.RemoveAll(this);
	}

	SelectionComponent.Reset();
	SelectedSkill = nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(IsConfirmEnabled, false);
	UE_MVVM_SET_PROPERTY_VALUE(
		SkillEntries,
		TArray<TObjectPtr<UDRStartingSkillEntryViewModel>>());
}

void UDRStartingSkillViewModel::SelectSkill(
	UDRStartingSkillEntryViewModel* SkillEntry)
{
	if (!IsValid(SkillEntry) || !SkillEntries.Contains(SkillEntry))
	{
		return;
	}

	if (IsValid(SelectedSkill))
	{
		SelectedSkill->SetSelected(false);
	}

	SkillEntry->SetSelected(true);
	SelectedSkill = SkillEntry;
	UE_MVVM_SET_PROPERTY_VALUE(IsConfirmEnabled, true);
}
