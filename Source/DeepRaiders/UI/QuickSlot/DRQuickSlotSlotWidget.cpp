// Fill out your copyright notice in the Description page of Project Settings.


#include "DRQuickSlotSlotWidget.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

void UDRQuickSlotSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SlotButton->OnClicked.AddDynamic(this, &ThisClass::HandleSlotClicked);
}

void UDRQuickSlotSlotWidget::NativeDestruct()
{
	SlotButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotClicked);
	
	Super::NativeDestruct();
}

void UDRQuickSlotSlotWidget::SetSlotData(int32 NewSlotIndex, UDRItemDefinition* Definition, int32 Quantity,
	bool bIsSelected, bool bIsAvailable)
{
	SlotIndex = NewSlotIndex;
	
	SlotNumberText->SetText(FText::AsNumber(SlotIndex + 1));
	
	UTexture2D* Icon = IsValid(Definition) ? Definition->Icon.Get() : nullptr;
	
	ItemIcon->SetBrushFromTexture(Icon);
	ItemIcon->SetVisibility(IsValid(Icon) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	
	ItemIcon->SetRenderOpacity(bIsAvailable ? 1.f : 0.35f);
	
	QuantityText->SetText(IsValid(Definition) ? FText::AsNumber(Quantity) : FText::GetEmpty());
	
	SelectionBorder->SetBrushColor(bIsSelected ? SelectedColor : UnselectedColor);	
}

void UDRQuickSlotSlotWidget::HandleSlotClicked()
{
	if (SlotIndex != INDEX_NONE)
	{
		OnQuickSlotClickedDelegate.Broadcast(SlotIndex);
	}
}
