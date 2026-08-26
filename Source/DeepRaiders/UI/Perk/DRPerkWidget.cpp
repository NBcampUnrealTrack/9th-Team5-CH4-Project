#include "DRPerkWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ScaleBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "DeepRaiders/UI/ViewModel/DRPerkViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRPerkWidget::InitializePerks(UDRPerkComponent* NewPerkComponent)
{
	if (!IsValid(PerkViewModel))
	{
		PerkViewModel = NewObject<UDRPerkViewModel>(this);
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!IsValid(View)
		|| !View->SetViewModel(PerkViewModelName, PerkViewModel))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Perk ViewModel '%s' was not registered on %s"),
			*PerkViewModelName.ToString(),
			*GetName());
		return;
	}

	PerkViewModel->Initialize(NewPerkComponent);
}

void UDRPerkWidget::SetPerkEntries(
	const TArray<UDRPerkEntryViewModel*>& NewPerkEntries)
{
	if (!IsValid(SlotPanel) || !SlotWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Perk UI setup is invalid. SlotPanel=%s, SlotWidgetClass=%s"),
			*GetNameSafe(SlotPanel),
			*GetNameSafe(SlotWidgetClass));
		return;
	}

	SlotPanel->ClearChildren();

	const int32 ColumnCount = FMath::Max(1, PerkSlotsPerRow);
	for (int32 Index = 0; Index < NewPerkEntries.Num(); ++Index)
	{
		UDRPerkEntryViewModel* EntryViewModel = NewPerkEntries[Index];
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UUserWidget* SlotWidget = CreateWidget<UUserWidget>(
			GetOwningPlayer(),
			SlotWidgetClass);
		if (!IsValid(SlotWidget))
		{
			continue;
		}

		UMVVMView* EntryView = UMVVMSubsystem::GetViewFromUserWidget(SlotWidget);
		if (!IsValid(EntryView)
			|| !EntryView->SetViewModel(EntryViewModelName, EntryViewModel))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Perk Entry ViewModel '%s' was not registered on %s"),
				*EntryViewModelName.ToString(),
				*GetNameSafe(SlotWidget));
			continue;
		}

		UScaleBox* SlotScaleBox = WidgetTree->ConstructWidget<UScaleBox>();
		SlotScaleBox->SetStretch(EStretch::ScaleToFit);
		SlotScaleBox->SetStretchDirection(EStretchDirection::Both);
		SlotScaleBox->AddChild(SlotWidget);

		UUniformGridSlot* GridSlot = SlotPanel->AddChildToUniformGrid(
			SlotScaleBox,
			Index / ColumnCount,
			Index % ColumnCount);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UDRPerkWidget::NativeDestruct()
{
	if (IsValid(PerkViewModel))
	{
		PerkViewModel->Deinitialize();
	}

	Super::NativeDestruct();
}
