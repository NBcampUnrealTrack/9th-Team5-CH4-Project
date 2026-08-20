#include "DRPerkWidget.h"

#include "Blueprint/WidgetTree.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/UI/Perk/DRPerkSlotWidget.h"

void UDRPerkWidget::InitializePerks(
	UDRPerkComponent* NewPerkComponent)
{
	CachePerkSlots();

	if (PerkComponent == NewPerkComponent)
	{
		RefreshPerks();
		return;
	}

	UnbindPerkComponent();
	PerkComponent = NewPerkComponent;

	if (IsValid(PerkComponent))
	{
		PerkComponent->OnPerksChanged.AddUniqueDynamic(
			this,
			&ThisClass::RefreshPerks);
	}

	RefreshPerks();
}

void UDRPerkWidget::NativeDestruct()
{
	UnbindPerkComponent();
	PerkSlots.Reset();
	Super::NativeDestruct();
}

void UDRPerkWidget::CachePerkSlots()
{
	if (!PerkSlots.IsEmpty() || !IsValid(WidgetTree))
	{
		return;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (UDRPerkSlotWidget* PerkSlot = Cast<UDRPerkSlotWidget>(Widget))
		{
			PerkSlots.Add(PerkSlot);
		}
	}
}

void UDRPerkWidget::RefreshPerks()
{
	const int32 MaxPerkSlotCount = IsValid(PerkComponent)
		? PerkComponent->GetMaxPerkSlotCount()
		: 0;
	const TArray<FDRPerkEntry>* PerkEntries = IsValid(PerkComponent)
		? &PerkComponent->GetPerkEntries()
		: nullptr;

	for (int32 SlotIndex = 0;
		SlotIndex < PerkSlots.Num();
		++SlotIndex)
	{
		UDRPerkSlotWidget* PerkSlot = PerkSlots[SlotIndex].Get();
		if (!IsValid(PerkSlot))
		{
			continue;
		}

		const bool IsAvailableSlot = SlotIndex < MaxPerkSlotCount;
		PerkSlot->SetVisibility(
			IsAvailableSlot
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);

		const UDRPerkDefinition* PerkDefinition =
			IsAvailableSlot && PerkEntries
				&& PerkEntries->IsValidIndex(SlotIndex)
				? (*PerkEntries)[SlotIndex].PerkDefinition
				: nullptr;
		PerkSlot->SetPerkDefinition(PerkDefinition);
	}
}

void UDRPerkWidget::UnbindPerkComponent()
{
	if (IsValid(PerkComponent))
	{
		PerkComponent->OnPerksChanged.RemoveDynamic(
			this,
			&ThisClass::RefreshPerks);
	}

	PerkComponent = nullptr;
}
