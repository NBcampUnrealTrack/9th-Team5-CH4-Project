// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventorySlotWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "DRInventoryDragDropOperation.h"
#include "InputCoreTypes.h"

void UDRInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (ensure(IsValid(SlotButton)))
	{
		SlotButton->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UDRInventorySlotWidget::SetItemInstance(int32 InSlotIndex, const FDRItemInstance& ItemInstance, bool bInLocked)
{
	SlotIndex = InSlotIndex;
	bLocked = bInLocked;
	
	if (!ItemInstance.IsValid())
	{
		ClearSlot(InSlotIndex, bInLocked);
		return;
	}
	
	InstanceId = ItemInstance.InstanceId;
	
	UTexture2D* Icon = IsValid(ItemInstance.Definition) ? ItemInstance.Definition->Icon : nullptr;
	
	ItemIcon->SetBrushFromTexture(Icon);
	
	ItemIcon->SetVisibility(IsValid(Icon) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	
	QuantityText->SetText(FText::AsNumber(ItemInstance.Quantity));
	QuantityText->SetVisibility(ItemInstance.Quantity > 1 
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UDRInventorySlotWidget::ClearSlot(int32 InSlotIndex, bool bInLocked)
{
	SlotIndex = InSlotIndex;
	bLocked = bInLocked;
	InstanceId.Invalidate();
	
	ItemIcon->SetBrushFromTexture(nullptr);
	ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	
	QuantityText->SetText(FText::GetEmpty());
	QuantityText->SetVisibility(ESlateVisibility::Hidden);	
}

FReply UDRInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InstanceId.IsValid() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	
	bPointerPressed = true;
	
	if (bLocked)
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	
	FReply DragReply = UWidgetBlueprintLibrary::DetectDragIfPressed(
		InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	
	return DragReply.CaptureMouse(TakeWidget());	
}

FReply UDRInventorySlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bPointerPressed)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	
	bPointerPressed = false;
	
	const bool bReleasedInside = InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	
	if (bReleasedInside && InstanceId.IsValid())
	{
		HandleSlotClicked();
	}
	
	return FReply::Handled().ReleaseMouseCapture();
}

void UDRInventorySlotWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bPointerPressed = false;
	
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UDRInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                                  UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	
	bPointerPressed = false;
	
	if (bLocked
		|| !InstanceId.IsValid()
		|| SlotIndex == INDEX_NONE)
	{
		return;
	}
	
	UDRInventoryDragDropOperation* Operation = Cast<UDRInventoryDragDropOperation>(
		UWidgetBlueprintLibrary::CreateDragDropOperation(UDRInventoryDragDropOperation::StaticClass()));
	
	if (!Operation)
	{
		return;
	}
	
	Operation->SourceSlotIndex = SlotIndex;
	Operation->SourceInstanceId = InstanceId;
	Operation->Pivot = EDragPivot::CenterCenter;
	
	if (IsValid(ItemIcon))
	{
		UImage* DragVisual = NewObject<UImage>(Operation);
		
		if (IsValid(DragVisual))
		{
			DragVisual->SetBrush(ItemIcon->GetBrush());
			
			FLinearColor DragVisualColor = ItemIcon->GetColorAndOpacity();
			DragVisualColor.A *= DragVisualOpacity;
			
			DragVisual->SetColorAndOpacity(DragVisualColor);
			DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
			
			const FVector2D IconSize = ItemIcon->GetCachedGeometry().GetLocalSize();
			
			if (!IconSize.IsNearlyZero())
			{
				DragVisual->SetDesiredSizeOverride(IconSize);
			}
			
			Operation->DefaultDragVisual = DragVisual;
		}
	}
	
	
	OutOperation = Operation;
}

bool UDRInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (bLocked || SlotIndex == INDEX_NONE)
	{
		return false;
	}
	
	const UDRInventoryDragDropOperation* Operation = Cast<UDRInventoryDragDropOperation>(InOperation);
	
	if (!Operation
		|| !Operation->SourceInstanceId.IsValid()
		|| Operation->SourceSlotIndex == SlotIndex)
	{
		return false;
	}
	
	OnMoveRequestedDelegate.Broadcast(Operation->SourceSlotIndex, SlotIndex);
	
	return true;
}

void UDRInventorySlotWidget::HandleSlotClicked()
{
	if(InstanceId.IsValid())
	{
		OnSlotClickedDelegate.Broadcast(InstanceId);
	}
}
