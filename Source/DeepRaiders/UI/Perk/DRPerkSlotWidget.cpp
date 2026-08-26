#include "DRPerkSlotWidget.h"

#include "DeepRaiders/UI/ViewModel/DRPerkViewModel.h"
#include "InputCoreTypes.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRPerkSlotWidget::InitializeViewModel(UDRPerkEntryViewModel* NewViewModel)
{
	EntryViewModel = NewViewModel;
	PerkInstanceId.Invalidate();
	if (!IsValid(EntryViewModel))
	{
		return;
	}

	PerkInstanceId = EntryViewModel->GetPerkInstanceId();
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!IsValid(View)
		|| !View->SetViewModel(EntryViewModelName, EntryViewModel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Perk Entry ViewModel '%s' was not registered on %s"),
			*EntryViewModelName.ToString(), *GetName());
	}
}

FReply UDRPerkSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!PerkInstanceId.IsValid()
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	IsPointerPressed = true;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UDRPerkSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsPointerPressed || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	IsPointerPressed = false;
	if (PerkInstanceId.IsValid() && InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		OnSlotClicked.Broadcast(PerkInstanceId);
	}

	return FReply::Handled().ReleaseMouseCapture();
}

void UDRPerkSlotWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	IsPointerPressed = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
