#include "DRInventoryScreenWidget.h"

#include "Components/Button.h"
#include "DRInventoryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRInventoryViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRInventoryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}

	const TArray<UDRInventoryWidget*> Panels = {
		LeftInventoryPanel.Get(),
		CenterInventoryPanel.Get(),
		RightInventoryPanel.Get()};
	for (UDRInventoryWidget* Panel : Panels)
	{
		if (IsValid(Panel))
		{
			Panel->OnEntryClickedDelegate.AddUniqueDynamic(this, &ThisClass::HandleEntryClicked);
		}
	}
}

void UDRInventoryScreenWidget::NativeDestruct()
{
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	const TArray<UDRInventoryWidget*> Panels = {
		LeftInventoryPanel.Get(),
		CenterInventoryPanel.Get(),
		RightInventoryPanel.Get()};
	for (UDRInventoryWidget* Panel : Panels)
	{
		if (IsValid(Panel))
		{
			Panel->OnEntryClickedDelegate.RemoveDynamic(this, &ThisClass::HandleEntryClicked);
		}
	}
	Super::NativeDestruct();
}

void UDRInventoryScreenWidget::InitializeScreen(APlayerController* PlayerController)
{
	ScreenViewModel = NewObject<UDRInventoryScreenViewModel>(this);
	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
		IsValid(View) && View->SetViewModel(ScreenViewModelName, ScreenViewModel))
	{
		ScreenViewModel->Initialize(PlayerController);
	}
}

void UDRInventoryScreenWidget::SetLeftPanel(UDRInventoryViewModel* PanelViewModel)
{
	InitializePanel(LeftInventoryPanel, PanelViewModel);
}

void UDRInventoryScreenWidget::SetCenterPanel(UDRInventoryViewModel* PanelViewModel)
{
	InitializePanel(CenterInventoryPanel, PanelViewModel);
}

void UDRInventoryScreenWidget::SetRightPanel(UDRInventoryViewModel* PanelViewModel)
{
	InitializePanel(RightInventoryPanel, PanelViewModel);
}

void UDRInventoryScreenWidget::InitializePanel(
	UDRInventoryWidget* Panel,
	UDRInventoryViewModel* PanelViewModel)
{
	if (IsValid(Panel) && IsValid(PanelViewModel))
	{
		Panel->InitializeViewModel(PanelViewModel);
	}
}

void UDRInventoryScreenWidget::HandleEntryClicked(FGuid InstanceId)
{
	if (InstanceId.IsValid())
	{
		OnEntryClickedDelegate.Broadcast(InstanceId);
	}
}

void UDRInventoryScreenWidget::HandleCloseClicked()
{
	OnCloseRequestedDelegate.Broadcast();
}
