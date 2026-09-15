#include "DRQuickSlotViewModel.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"


constexpr float ActivationIntervalRefreshRate = 1.0f / 60.f;

void UDRQuickSlotEntryViewModel::SelectSlot()
{
	if (QuickSlotComponent.IsValid())
	{
		QuickSlotComponent->RequestSelectSlot(SlotIndex);
	}
}

void UDRQuickSlotEntryViewModel::Initialize(
	UDRQuickSlotComponent* InQuickSlotComponent,
	int32 InSlotIndex)
{
	QuickSlotComponent = InQuickSlotComponent;
	UE_MVVM_SET_PROPERTY_VALUE(SlotIndex, InSlotIndex);
	UE_MVVM_SET_PROPERTY_VALUE(SlotNumberText, FText::AsNumber(InSlotIndex + 1));
	Refresh();
}

void UDRQuickSlotEntryViewModel::Refresh()
{
	if (!QuickSlotComponent.IsValid())
	{
		return;
	}

	FDRItemInstance ItemInstance;
	const bool bNewHasItem = QuickSlotComponent->GetQuickSlot(SlotIndex, ItemInstance);

	UDRItemDefinition* NewItemDefinition = bNewHasItem ? ItemInstance.Definition.Get() : nullptr;
	UTexture2D* NewItemIcon = bNewHasItem ? NewItemDefinition->Icon.Get() : nullptr;
	const int32 NewQuantity = bNewHasItem ? ItemInstance.Quantity : 0;

	UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, NewItemDefinition);
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, NewItemIcon);
	UE_MVVM_SET_PROPERTY_VALUE(Quantity, NewQuantity);
	UE_MVVM_SET_PROPERTY_VALUE(
		QuantityText,
		bNewHasItem ? FText::AsNumber(NewQuantity) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bHasItem, bNewHasItem);
	UE_MVVM_SET_PROPERTY_VALUE(
		bIsSelected,
		QuickSlotComponent->GetSelectedSlotIndex() == SlotIndex);
	UE_MVVM_SET_PROPERTY_VALUE(
		bIsAvailable,
		QuickSlotComponent->IsSlotItemAvailable(SlotIndex));
	
	RefreshActivationInterval();
}

bool UDRQuickSlotEntryViewModel::RefreshActivationInterval()
{
	float NewProgress = 0.0f;
	const bool bNewIsActive = QuickSlotComponent.IsValid()
		&& QuickSlotComponent->GetQuickSlotActivationIntervalState(SlotIndex, NewProgress);
	
	// UI에게 진행도를 역으로 전달
	NewProgress = FMath::Clamp(1.0f - NewProgress, 0.0f, 1.0f);
	
	UE_MVVM_SET_PROPERTY_VALUE(ActivationIntervalProgress, NewProgress);
	UE_MVVM_SET_PROPERTY_VALUE(bIsActivationIntervalActive, bNewIsActive);

	return bNewIsActive;
}

void UDRQuickSlotViewModel::Initialize(UDRQuickSlotComponent* InQuickSlotComponent)
{
	Deinitialize();
	QuickSlotComponent = InQuickSlotComponent;

	if (!QuickSlotComponent.IsValid())
	{
		return;
	}

	QuickSlotComponent->OnQuickSlotsChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleQuickSlotsChanged);
	QuickSlotComponent->OnQuickSlotCountChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleQuickSlotCountChanged);
	QuickSlotComponent->OnSelectedQuickSlotIndexChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleSelectedSlotChanged);
	QuickSlotComponent->OnQuickSlotActivationIntervalChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleActivationIntervalChanged);

	RebuildSlotEntries();
	RefreshActivationIntervals();
}

void UDRQuickSlotViewModel::Deinitialize()
{
	if (QuickSlotComponent.IsValid())
	{
		QuickSlotComponent->OnQuickSlotsChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleQuickSlotsChanged);
		QuickSlotComponent->OnQuickSlotCountChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleQuickSlotCountChanged);
		QuickSlotComponent->OnSelectedQuickSlotIndexChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleSelectedSlotChanged);
		QuickSlotComponent->OnQuickSlotActivationIntervalChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleActivationIntervalChanged);
	}

	QuickSlotComponent.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(
		SlotEntries,
		TArray<TObjectPtr<UDRQuickSlotEntryViewModel>>());
}

void UDRQuickSlotViewModel::HandleQuickSlotsChanged()
{
	// 슬롯 이동 시 엔트리를 다시 생성해 아이콘과 수량을 새 위치로 초기화한다.
	RebuildSlotEntries();
}

void UDRQuickSlotViewModel::HandleQuickSlotCountChanged(int32)
{
	RebuildSlotEntries();
}

void UDRQuickSlotViewModel::HandleSelectedSlotChanged(
	int32,
	int32)
{
	RefreshSlotEntries();
}

void UDRQuickSlotViewModel::RebuildSlotEntries()
{
	TArray<TObjectPtr<UDRQuickSlotEntryViewModel>> NewSlotEntries;

	if (QuickSlotComponent.IsValid())
	{
		NewSlotEntries.Reserve(QuickSlotComponent->GetSlotCount());

		for (int32 SlotIndex = 0; SlotIndex < QuickSlotComponent->GetSlotCount(); ++SlotIndex)
		{
			UDRQuickSlotEntryViewModel* EntryViewModel = NewObject<UDRQuickSlotEntryViewModel>(this);
			EntryViewModel->Initialize(QuickSlotComponent.Get(), SlotIndex);
			NewSlotEntries.Add(EntryViewModel);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(SlotEntries, MoveTemp(NewSlotEntries));
}

void UDRQuickSlotViewModel::RefreshSlotEntries()
{
	for (UDRQuickSlotEntryViewModel* EntryViewModel : SlotEntries)
	{
		if (IsValid(EntryViewModel))
		{
			EntryViewModel->Refresh();
		}
	}
}

void UDRQuickSlotViewModel::HandleActivationIntervalChanged()
{
	RefreshActivationIntervals();
}

void UDRQuickSlotViewModel::RefreshActivationIntervals()
{
	bool bHasActiveInterval = false;

	for (UDRQuickSlotEntryViewModel* EntryViewModel : SlotEntries)
	{
		if (IsValid(EntryViewModel))
		{
			bHasActiveInterval |= EntryViewModel->RefreshActivationInterval();
		}
	}

	UWorld* World = QuickSlotComponent.IsValid() ? QuickSlotComponent->GetWorld() : nullptr;

	if (bHasActiveInterval && IsValid(World))
	{
		if (!World->GetTimerManager().IsTimerActive(ActivationIntervalTimerHandle))
		{
			World->GetTimerManager().SetTimer(ActivationIntervalTimerHandle, this,
				&ThisClass::RefreshActivationIntervals, ActivationIntervalRefreshRate, true);
		}
	}
	else
	{
		StopActivationIntervalTimer();
	}
}

void UDRQuickSlotViewModel::StopActivationIntervalTimer()
{
	UWorld* World = QuickSlotComponent.IsValid() ? QuickSlotComponent->GetWorld() : nullptr;

	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(ActivationIntervalTimerHandle);
	}

	ActivationIntervalTimerHandle.Invalidate();
}
