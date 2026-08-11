// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventoryWidget.h"

#include "Components/Button.h"
#include "Components/UniformGridPanel.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DRInventorySlotWidget.h"

void UDRInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UDRInventoryWidget::NativeDestruct()
{
	UnBindInventory();
	
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	
	Super::NativeDestruct();
}

void UDRInventoryWidget::InitializeInventory(UDRInventoryComponent* NewInventoryComponent)
{
	UnBindInventory();
	InventoryComponent= NewInventoryComponent;
	
	BindInventory();
	RebuildSlot();
	RefreshSlots();
}

void UDRInventoryWidget::BindInventory()
{
	if (UDRInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->OnInventoryChangedDelegate.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	}
}

void UDRInventoryWidget::UnBindInventory()
{
	if (UDRInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->OnInventoryChangedDelegate.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
}

void UDRInventoryWidget::RebuildSlot()
{
	UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	if (!IsValid(Inventory)
		|| !IsValid(SlotPanel)
		||!InventorySlotWidgetClass)
	{
		return;
	}
	
	SlotPanel->ClearChildren();
	SlotWidgets.Reset();
	
	const int32 SlotCount = Inventory->GetMaxSlots();
	const int32 ColumnCount = FMath::Max(1, SlotsPerRow);
	
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UDRInventorySlotWidget* SlotWidget = CreateWidget<UDRInventorySlotWidget>(GetOwningPlayer(), InventorySlotWidgetClass);
		
		// 생성 실패 시 무시하고 진행
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		
		SlotWidget->OnSlotClickedDelegate.AddDynamic(this, &ThisClass::HandleSlotClicked);
		
		SlotPanel->AddChildToUniformGrid(SlotWidget, SlotIndex / ColumnCount, SlotIndex % ColumnCount);
		
		SlotWidgets.Add(SlotWidget);		
	}
}

void UDRInventoryWidget::RefreshSlots()
{
	UDRInventoryComponent* Inventory = InventoryComponent.Get();
	
	if (!IsValid(Inventory))
	{
		return;
	}
	
	const TArray<FDRInventoryEntry> Entries = Inventory->GetEntries();
	
	for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
	{
		UDRInventorySlotWidget* SlotWidget = SlotWidgets[SlotIndex];
		
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		
		// 현재 구현 상 InventoryComponent::Entries의 Index와 SlotIndex가 1:1 매칭된다.
		// 슬롯의 위치가 고정되어 있지 않고, 앞 쪽 슬롯이 빌 시 앞으로 당겨진다.
		if (Entries.IsValidIndex(SlotIndex))
		{
			SlotWidget->SetEntry(Entries[SlotIndex]);
		}
		else
		{
			SlotWidget->ClearSlot();
		}
	}
}

void UDRInventoryWidget::HandleInventoryChanged()
{
	RefreshSlots();
}

void UDRInventoryWidget::HandleSlotClicked(FGuid EntryId)
{
	OnEntryClickedDelegate.Broadcast(EntryId);
}

void UDRInventoryWidget::HandleCloseClicked()
{
	OnCloseRequestedDelegate.Broadcast();
}
