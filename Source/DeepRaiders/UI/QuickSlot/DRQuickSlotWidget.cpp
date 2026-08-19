// Fill out your copyright notice in the Description page of Project Settings.


#include "DRQuickSlotWidget.h"

#include "Components/HorizontalBox.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/UI/QuickSlot/DRQuickSlotSlotWidget.h"
#include "DeepRaiders/UI/ViewModel/DRQuickSlotViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRQuickSlotWidget::NativeDestruct()
{
	if (IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->Deinitialize();
	}

	Super::NativeDestruct();
}

void UDRQuickSlotWidget::InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent)
{
	if (!IsValid(NewQuickSlotComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlotComponent is invalid on %s"), *GetName());
		return;
	}

	if (!IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel = NewObject<UDRQuickSlotViewModel>(this);
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!IsValid(View)
		|| !View->SetViewModel(QuickSlotViewModelName, QuickSlotViewModel))
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlotViewModel '%s' was not registered on %s"),
			*QuickSlotViewModelName.ToString(), *GetName());
		return;
	}

	QuickSlotViewModel->Initialize(NewQuickSlotComponent);
}

void UDRQuickSlotWidget::SetSlotEntries(
	const TArray<UDRQuickSlotEntryViewModel*>& NewSlotEntries)
{
	if (!IsValid(SlotPanel) || !SlotWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlot UI setup is invalid. SlotPanel=%s, SlotWidgetClass=%s"),
			*GetNameSafe(SlotPanel), *GetNameSafe(SlotWidgetClass));
		return;
	}

	SlotPanel->ClearChildren();

	for (UDRQuickSlotEntryViewModel* EntryViewModel : NewSlotEntries)
	{
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UDRQuickSlotSlotWidget* SlotWidget = CreateWidget<UDRQuickSlotSlotWidget>(
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
			UE_LOG(LogTemp, Error, TEXT("Entry ViewModel '%s' was not registered on %s"),
				*EntryViewModelName.ToString(), *GetNameSafe(SlotWidget));
			continue;
		}

		SlotPanel->AddChildToHorizontalBox(SlotWidget);
	}
}
