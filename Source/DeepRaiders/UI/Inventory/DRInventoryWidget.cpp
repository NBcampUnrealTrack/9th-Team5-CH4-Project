// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventoryWidget.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/UI/ViewModel/DRInventoryViewModel.h"
#include "DRInventorySlotWidget.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

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

	if (IsValid(InventoryViewModel))
	{
		InventoryViewModel->Deinitialize();
	}
	
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	
	Super::NativeDestruct();
}

void UDRInventoryWidget::InitializeInventory(UDRInventoryComponent* NewInventoryComponent)
{
	if (!IsValid(InventoryViewModel))
	{
		InventoryViewModel = NewObject<UDRInventoryViewModel>(this);
	}

	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
		IsValid(View) && View->SetViewModel(InventoryViewModelName, InventoryViewModel))
	{
		InventoryViewModel->Initialize(NewInventoryComponent);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory ViewModel '%s' is not registered on %s"),
			*InventoryViewModelName.ToString(), *GetName());
	}

	UnBindInventory();
	InventoryComponent= NewInventoryComponent;
	
	BindInventory();
	RebuildSlot();
	RefreshSlots();
}

void UDRInventoryWidget::InitializeViewModel(UDRInventoryViewModel* NewViewModel)
{
	if (!IsValid(NewViewModel))
	{
		return;
	}

	InventoryViewModel = NewViewModel;
	InventoryComponent = NewViewModel->GetInventoryComponent();
	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->SetViewModel(InventoryViewModelName, InventoryViewModel);
	}
}

void UDRInventoryWidget::SetSlotEntries(
	const TArray<UDRInventorySlotEntryViewModel*>& NewSlotEntries)
{
	if (!IsValid(SlotPanel) || !InventorySlotWidgetClass)
	{
		return;
	}

	SlotPanel->ClearChildren();
	SlotWidgets.Reset();
	const int32 ColumnCount = FMath::Max(1, SlotsPerRow);

	for (UDRInventorySlotEntryViewModel* EntryViewModel : NewSlotEntries)
	{
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UDRInventorySlotWidget* SlotWidget = CreateWidget<UDRInventorySlotWidget>(
			GetOwningPlayer(),
			InventorySlotWidgetClass);

		if (!IsValid(SlotWidget))
		{
			continue;
		}

		SlotWidget->InitializeViewModel(EntryViewModel);
		SlotWidget->OnMoveRequestedDelegate.AddDynamic(this, &ThisClass::HandleMoveRequested);
		SlotWidget->OnSlotClickedDelegate.AddDynamic(this, &ThisClass::HandleSlotClicked);
		const int32 SlotIndex = EntryViewModel->GetSlotIndex();
		SlotPanel->AddChildToUniformGrid(
			SlotWidget,
			SlotIndex / ColumnCount,
			SlotIndex % ColumnCount);
		SlotWidgets.Add(SlotWidget);
	}
}

void UDRInventoryWidget::SetQuickSlotEntries(
	const TArray<UDRInventorySlotEntryViewModel*>& NewQuickSlotEntries)
{
	if (!IsValid(QuickSlotPanel) || !QuickSlotWidgetClass)
	{
		return;
	}

	QuickSlotPanel->ClearChildren();
	for (UDRInventorySlotEntryViewModel* EntryViewModel : NewQuickSlotEntries)
	{
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UDRInventorySlotWidget* SlotWidget = CreateWidget<UDRInventorySlotWidget>(
			GetOwningPlayer(),
			QuickSlotWidgetClass);
		if (!IsValid(SlotWidget))
		{
			continue;
		}

		SlotWidget->InitializeViewModel(EntryViewModel);
		SlotWidget->OnSlotClickedDelegate.AddDynamic(this, &ThisClass::HandleSlotClicked);
		QuickSlotPanel->AddChild(SlotWidget);
	}
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
		
		if (!ensureMsgf(IsValid(SlotWidget), TEXT("Failed to create InventorySlotWidget at index %d"), SlotIndex))
		{
			SlotPanel->ClearChildren();
			SlotWidgets.Reset();
			return;
		}
		
		SlotWidget->OnMoveRequestedDelegate.AddDynamic(this, &ThisClass::HandleMoveRequested);
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
	
	const TArray<FDRItemInstance> ItemInstances = Inventory->GetItemInstances();
	
	for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
	{
		UDRInventorySlotWidget* SlotWidget = SlotWidgets[SlotIndex];
		
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		
		const bool bLocked = Inventory->IsSlotLocked(SlotIndex);
		
		// 현재 구현 상 InventoryComponent::Entries의 Index와 SlotIndex가 1:1 매칭된다.
		// 슬롯의 위치가 고정되어 있지 않고, 앞 쪽 슬롯이 빌 시 앞으로 당겨진다.
		if (ItemInstances.IsValidIndex(SlotIndex)
			&& ItemInstances[SlotIndex].IsValid())
		{
			SlotWidget->SetItemInstance(SlotIndex, ItemInstances[SlotIndex], bLocked);
		}
		else
		{
			SlotWidget->ClearSlot(SlotIndex, bLocked);
		}
	}
}

void UDRInventoryWidget::HandleInventoryChanged()
{
	RefreshSlots();
}

void UDRInventoryWidget::HandleSlotClicked(FGuid InstanceId)
{
	OnEntryClickedDelegate.Broadcast(InstanceId);
}

void UDRInventoryWidget::HandleMoveRequested(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
	if (UDRInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->RequestSwapSlots(SourceSlotIndex, TargetSlotIndex);
	}
}

void UDRInventoryWidget::HandleCloseClicked()
{
	OnCloseRequestedDelegate.Broadcast();
}
