// Fill out your copyright notice in the Description page of Project Settings.


#include "DRQuickSlotWidget.h"

#include "Components/UniformGridPanel.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DRQuickSlotSlotWidget.h"

void UDRQuickSlotWidget::NativeDestruct()
{	
	UnbindQuickSlot();
	
	Super::NativeDestruct();
}

void UDRQuickSlotWidget::InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent)
{
	UnbindQuickSlot();
	QuickSlotComponent = NewQuickSlotComponent;
	
	BindQuickSlot();
	RebuildSlots();
	RefreshSlots();
}

void UDRQuickSlotWidget::BindQuickSlot()
{
	if (UDRQuickSlotComponent* QuickSlot = QuickSlotComponent.Get())
	{
		QuickSlot->OnQuickSlotsChangedDelegate.AddDynamic(this, &ThisClass::HandleQuickSlotsChanged);
		QuickSlot->OnQuickSlotCountChangedDelegate.AddDynamic(this, &ThisClass::HandleQuickSlotCountChanged);
		QuickSlot->OnSelectedQuickSlotIndexChangedDelegate.AddDynamic(this, &ThisClass::HandleSelectedSlotChanged);
	}
}

void UDRQuickSlotWidget::UnbindQuickSlot()
{
	if (UDRQuickSlotComponent* QuickSlot = QuickSlotComponent.Get())
	{
		QuickSlot->OnQuickSlotsChangedDelegate.RemoveDynamic(this, &ThisClass::HandleQuickSlotsChanged);
		QuickSlot->OnQuickSlotCountChangedDelegate.RemoveDynamic(this, &ThisClass::HandleQuickSlotCountChanged);
		QuickSlot->OnSelectedQuickSlotIndexChangedDelegate.RemoveDynamic(this, &ThisClass::HandleSelectedSlotChanged);
	}
}

void UDRQuickSlotWidget::RebuildSlots()
{
	UDRQuickSlotComponent* QuickSlot = QuickSlotComponent.Get();
	
	if (!IsValid(QuickSlot)
		|| !IsValid(SlotPanel)
		|| !QuickSlotSlotWidgetClass)
	{
		return;
	}
	
	SlotPanel->ClearChildren();
	SlotWidgets.Reset();
	
	for (int32 SlotIndex = 0; SlotIndex < QuickSlot->GetSlotCount(); ++SlotIndex)
	{
		UDRQuickSlotSlotWidget* SlotWidget = CreateWidget<UDRQuickSlotSlotWidget>(GetOwningPlayer(), QuickSlotSlotWidgetClass);
		
		if (!ensureMsgf(IsValid(SlotWidget), TEXT("Failed to create QuickSlotWidget at index %d"), SlotIndex))
		{
			SlotPanel->ClearChildren();
			SlotWidgets.Reset();
			return;
		}
		
		SlotWidget->OnQuickSlotClickedDelegate.AddDynamic(this, &ThisClass::HandleSlotClicked);
		
		SlotPanel->AddChildToUniformGrid(SlotWidget, 0, SlotIndex);
		
		SlotWidgets.Add(SlotWidget);
	}
}

void UDRQuickSlotWidget::RefreshSlots()
{
	UDRQuickSlotComponent* QuickSlot = QuickSlotComponent.Get();
	
	if (!IsValid(QuickSlot))
	{
		return;
	}
	
	for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
	{
		FDRQuickSlotEntry Entry;
		QuickSlot->GetQuickSlot(SlotIndex, Entry);
		
		SlotWidgets[SlotIndex]->SetSlotData(SlotIndex, Entry.Definition, QuickSlot->GetSlotItemCount(SlotIndex)
			, QuickSlot->GetSelectedSlotIndex() == SlotIndex, QuickSlot->IsSlotItemAvailable(SlotIndex));
	}
}

void UDRQuickSlotWidget::HandleQuickSlotsChanged()
{
	RefreshSlots();
}

void UDRQuickSlotWidget::HandleQuickSlotCountChanged(int32 NewSlotCount)
{
	RebuildSlots();
	RefreshSlots();
}

void UDRQuickSlotWidget::HandleSelectedSlotChanged(int32 PreviousSlotIndex, int32 NewSlotIndex)
{
	RefreshSlots();
}

void UDRQuickSlotWidget::HandleSlotClicked(int32 SlotIndex)
{
	if (UDRQuickSlotComponent* QuickSlot = QuickSlotComponent.Get())
	{
		QuickSlot->RequestSelectSlot(SlotIndex);
	}
}
