#include "DRTeleportListItemWidget.h"

#include "Components/TextBlock.h"
#include "DeepRaiders/teleport/DRTeleportPoint.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

void UDRTeleportListItemWidget::SetTeleportPoint(ADRTeleportPoint* NewTeleportPoint)
{
	TeleportPoint = NewTeleportPoint;

	if (bIsWidgetConstructed)
	{
		ApplyTeleportPoint();
	}
}

void UDRTeleportListItemWidget::SetSelected(bool bNewSelected)
{
	if (bIsSelected == bNewSelected)
	{
		return;
	}

	bIsSelected = bNewSelected;
	BP_OnSelectedChanged(bIsSelected);
}

void UDRTeleportListItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bIsWidgetConstructed = true;

	ApplyTeleportPoint();
	BP_OnSelectedChanged(bIsSelected);
}

void UDRTeleportListItemWidget::NativeDestruct()
{
	bIsWidgetConstructed = false;
	Super::NativeDestruct();
}

FReply UDRTeleportListItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsValid(TeleportPoint))
	{
		OnTeleportSelected.Broadcast(TeleportPoint);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UDRTeleportListItemWidget::ApplyTeleportPoint()
{
	if (!IsValid(TextBlock_Name) || !IsValid(TeleportPoint))
	{
		return;
	}

	TextBlock_Name->SetText(TeleportPoint->GetTeleportDisplayName());
}
