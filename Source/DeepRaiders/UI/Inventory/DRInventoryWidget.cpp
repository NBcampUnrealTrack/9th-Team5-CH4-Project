// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventoryWidget.h"

#include "Components/Button.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/UI/Perk/DRPerkWidget.h"
#include "DeepRaiders/UI/ViewModel/DRInventoryViewModel.h"
#include "DRInventorySlotWidget.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UDRInventoryWidget::NativeDestruct()
{
	if (IsValid(InventoryViewModel))
	{
		InventoryViewModel->Deinitialize();
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

	InventoryComponent = NewInventoryComponent;
	ADRPlayerState* PlayerState = IsValid(InventoryViewModel)
		? InventoryViewModel->GetPlayerState()
		: nullptr;
	if (IsValid(PerkWidget))
	{
		PerkWidget->InitializePerks(
			IsValid(PlayerState) ? PlayerState->GetPerkComponent() : nullptr);
	}
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
		if (!View->SetViewModel(InventoryViewModelName, InventoryViewModel))
		{
			UE_LOG(LogTemp, Warning, TEXT("Inventory ViewModel '%s' is not registered on %s"),
				*InventoryViewModelName.ToString(), *GetName());
		}
	}

	// Manual ViewModel 주입 시 초기 배열 바인딩이 누락되지 않도록 즉시 반영한다.
	SetQuickSlotEntries(NewViewModel->GetQuickSlotEntries());
	ADRPlayerState* PlayerState = NewViewModel->GetPlayerState();
	if (IsValid(PerkWidget))
	{
		PerkWidget->InitializePerks(
			IsValid(PlayerState) ? PlayerState->GetPerkComponent() : nullptr);
	}
}

void UDRInventoryWidget::SetQuickSlotEntries(
	const TArray<UDRInventorySlotEntryViewModel*>& NewQuickSlotEntries)
{
	if (!IsValid(QuickSlotPanel) || !QuickSlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Quick slot UI is not configured on %s"), *GetName());
		return;
	}

	QuickSlotPanel->ClearChildren();
	const int32 ColumnCount = FMath::Max(1, QuickSlotsPerRow);
	for (int32 Index = 0; Index < NewQuickSlotEntries.Num(); ++Index)
	{
		UDRInventorySlotEntryViewModel* EntryViewModel = NewQuickSlotEntries[Index];
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
		SlotWidget->OnMoveRequestedDelegate.AddDynamic(this, &ThisClass::HandleMoveRequested);

		UUniformGridSlot* GridSlot = QuickSlotPanel->AddChildToUniformGrid(
			SlotWidget,
			Index / ColumnCount,
			Index % ColumnCount);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
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
