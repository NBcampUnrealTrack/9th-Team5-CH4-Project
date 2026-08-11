// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventorySlotWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

void UDRInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SlotButton->OnClicked.AddDynamic(this, &ThisClass::HandleSlotClicked);
}

void UDRInventorySlotWidget::NativeDestruct()
{
	SlotButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotClicked);
	
	Super::NativeDestruct();
}

void UDRInventorySlotWidget::SetEntry(const FDRInventoryEntry& Entry)
{
	EntryId = Entry.EntryId;
	
	UTexture2D* Icon = IsValid(Entry.Definition) ? Entry.Definition->Icon : nullptr;
	
	ItemIcon->SetBrushFromTexture(Icon);
	ItemIcon->SetVisibility(IsValid(Icon) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	
	QuantityText->SetText(FText::AsNumber(Entry.Quantity));
	QuantityText->SetVisibility((ESlateVisibility::HitTestInvisible));
}

void UDRInventorySlotWidget::ClearSlot()
{
	EntryId.Invalidate();
	
	ItemIcon->SetBrushFromTexture(nullptr);
	ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	
	QuantityText->SetText(FText::GetEmpty());
	QuantityText->SetVisibility((ESlateVisibility::Hidden));
}

void UDRInventorySlotWidget::HandleSlotClicked()
{
	if(EntryId.IsValid())
	{
		OnSlotClickedDelegate.Broadcast(EntryId);
	}
}
