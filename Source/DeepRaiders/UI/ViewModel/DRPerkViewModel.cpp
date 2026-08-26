#include "DRPerkViewModel.h"

#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"

void UDRPerkEntryViewModel::Initialize(
	FGuid NewPerkInstanceId,
	UDRPerkDefinition* NewPerkDefinition)
{
	const FText NewDisplayName = IsValid(NewPerkDefinition)
		? NewPerkDefinition->DisplayName
		: FText::GetEmpty();
	UTexture2D* NewIcon = IsValid(NewPerkDefinition)
		? NewPerkDefinition->Icon.Get()
		: nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(PerkInstanceId, NewPerkInstanceId);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);
}

void UDRPerkViewModel::Initialize(UDRPerkComponent* NewPerkComponent)
{
	Deinitialize();
	PerkComponent = NewPerkComponent;

	if (PerkComponent.IsValid())
	{
		PerkComponent->OnPerksChanged.AddDynamic(
			this,
			&ThisClass::RebuildPerkEntries);
	}

	RebuildPerkEntries();
}

void UDRPerkViewModel::Deinitialize()
{
	if (PerkComponent.IsValid())
	{
		PerkComponent->OnPerksChanged.RemoveDynamic(
			this,
			&ThisClass::RebuildPerkEntries);
	}

	PerkComponent.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(
		PerkEntries,
		TArray<TObjectPtr<UDRPerkEntryViewModel>>());
}

void UDRPerkViewModel::RebuildPerkEntries()
{
	TArray<TObjectPtr<UDRPerkEntryViewModel>> NewPerkEntries;

	if (PerkComponent.IsValid())
	{
		const TArray<FDRPerkEntry>& ComponentEntries =
			PerkComponent->GetPerkEntries();
		const int32 MaxPerkSlotCount = PerkComponent->GetMaxPerkSlotCount();
		NewPerkEntries.Reserve(MaxPerkSlotCount);

		for (int32 SlotIndex = 0; SlotIndex < MaxPerkSlotCount; ++SlotIndex)
		{
			const FDRPerkEntry* PerkEntry = ComponentEntries.IsValidIndex(SlotIndex)
				? &ComponentEntries[SlotIndex]
				: nullptr;
			UDRPerkDefinition* PerkDefinition = PerkEntry
				? PerkEntry->PerkDefinition.Get()
				: nullptr;
			UDRPerkEntryViewModel* EntryViewModel =
				NewObject<UDRPerkEntryViewModel>(this);
			EntryViewModel->Initialize(
				PerkEntry ? PerkEntry->PerkInstanceId : FGuid(),
				PerkDefinition);
			NewPerkEntries.Add(EntryViewModel);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(PerkEntries, MoveTemp(NewPerkEntries));
}
