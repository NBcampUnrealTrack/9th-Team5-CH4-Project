#include "DRTeleportSelectWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DeepRaiders/teleport/DRTeleportPoint.h"
#include "DRTeleportListItemWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UDRTeleportSelectWidget::InitializeTeleportList(ADRTeleportPoint* NewCurrentTeleportPoint, const TArray<ADRTeleportPoint*>& NewDestinationTeleportPoints)
{
	CurrentTeleportPoint = NewCurrentTeleportPoint;
	DestinationTeleportPoints.Reset();

	for (ADRTeleportPoint* DestinationTeleportPoint : NewDestinationTeleportPoints)
	{
		if (IsValid(DestinationTeleportPoint) && DestinationTeleportPoint != CurrentTeleportPoint)
		{
			DestinationTeleportPoints.AddUnique(DestinationTeleportPoint);
		}
	}

	RefreshCurrentTeleport();
	RefreshDestinationList();
}

void UDRTeleportSelectWidget::InitializeRegisteredTeleportList(int32 TeamId, ADRTeleportPoint* NewCurrentTeleportPoint)
{
	TArray<ADRTeleportPoint*> RegisteredDestinations;

	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		if (UDRTeleportComponent* TeleportComponent = OwningPawn->FindComponentByClass<UDRTeleportComponent>())
		{
			TeleportComponent->GetRegisteredTeleportDestinations(NewCurrentTeleportPoint, RegisteredDestinations);
		}
	}

	InitializeTeleportList(NewCurrentTeleportPoint, RegisteredDestinations);
}

void UDRTeleportSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(Button_Close))
	{
		Button_Close->OnClicked.AddDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(Button_Confirm))
	{
		Button_Confirm->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmButtonClicked);
	}
}

void UDRTeleportSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyUIInputMode();
}

void UDRTeleportSelectWidget::NativeDestruct()
{
	if (IsValid(Button_Close))
	{
		Button_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (IsValid(Button_Confirm))
	{
		Button_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmButtonClicked);
	}

	RestoreGameInputMode();
	Super::NativeDestruct();
}

void UDRTeleportSelectWidget::ApplyUIInputMode()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	bPreviousShowMouseCursor = PlayerController->bShowMouseCursor;
	PlayerController->bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}

void UDRTeleportSelectWidget::RestoreGameInputMode()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	PlayerController->bShowMouseCursor = bPreviousShowMouseCursor;
	PlayerController->SetInputMode(FInputModeGameOnly());
}

void UDRTeleportSelectWidget::RefreshCurrentTeleport()
{
	if (IsValid(TextBlock_CurrentTeleportName))
	{
		TextBlock_CurrentTeleportName->SetText(IsValid(CurrentTeleportPoint) ? CurrentTeleportPoint->GetTeleportDisplayName() : FText::GetEmpty());
	}
}

void UDRTeleportSelectWidget::RefreshDestinationList()
{
	if (!IsValid(ScrollBox_Destination))
	{
		return;
	}

	ClearSelectedDestination();
	ScrollBox_Destination->ClearChildren();

	if (!DestinationItemWidgetClass)
	{
		return;
	}

	for (ADRTeleportPoint* DestinationTeleportPoint : DestinationTeleportPoints)
	{
		if (!IsValid(DestinationTeleportPoint))
		{
			continue;
		}

		UDRTeleportListItemWidget* ItemWidget = CreateWidget<UDRTeleportListItemWidget>(GetOwningPlayer(), DestinationItemWidgetClass);
		if (!IsValid(ItemWidget))
		{
			continue;
		}

		ItemWidget->SetTeleportPoint(DestinationTeleportPoint);
		ItemWidget->OnTeleportSelected.AddDynamic(this, &ThisClass::HandleDestinationItemSelected);
		ScrollBox_Destination->AddChild(ItemWidget);
	}
}

void UDRTeleportSelectWidget::ClearSelectedDestination()
{
	if (IsValid(SelectedDestinationItemWidget))
	{
		SelectedDestinationItemWidget->SetSelected(false);
	}

	SelectedDestinationItemWidget = nullptr;
	SelectedDestinationTeleportPoint = nullptr;
}

UDRTeleportListItemWidget* UDRTeleportSelectWidget::FindItemWidgetByTeleportPoint(ADRTeleportPoint* TeleportPoint) const
{
	if (!IsValid(ScrollBox_Destination) || !IsValid(TeleportPoint))
	{
		return nullptr;
	}

	for (int32 ChildIndex = 0; ChildIndex < ScrollBox_Destination->GetChildrenCount(); ++ChildIndex)
	{
		UWidget* ChildWidget = ScrollBox_Destination->GetChildAt(ChildIndex);
		UDRTeleportListItemWidget* ItemWidget = Cast<UDRTeleportListItemWidget>(ChildWidget);
		if (IsValid(ItemWidget) && ItemWidget->GetTeleportPoint() == TeleportPoint)
		{
			return ItemWidget;
		}
	}

	return nullptr;
}

void UDRTeleportSelectWidget::HandleDestinationItemSelected(ADRTeleportPoint* DestinationTeleportPoint)
{
	UDRTeleportListItemWidget* ClickedItemWidget = FindItemWidgetByTeleportPoint(DestinationTeleportPoint);
	if (!IsValid(ClickedItemWidget))
	{
		return;
	}

	if (SelectedDestinationItemWidget == ClickedItemWidget)
	{
		ClearSelectedDestination();
		OnDestinationSelected.Broadcast(nullptr);
		return;
	}

	ClearSelectedDestination();
	SelectedDestinationItemWidget = ClickedItemWidget;
	SelectedDestinationTeleportPoint = DestinationTeleportPoint;
	SelectedDestinationItemWidget->SetSelected(true);
	OnDestinationSelected.Broadcast(DestinationTeleportPoint);
}

void UDRTeleportSelectWidget::HandleCloseButtonClicked()
{
	RemoveFromParent();
}

void UDRTeleportSelectWidget::HandleConfirmButtonClicked()
{
	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		if (UDRTeleportComponent* TeleportComponent = OwningPawn->FindComponentByClass<UDRTeleportComponent>())
		{
			TeleportComponent->RequestTeleportTo(SelectedDestinationTeleportPoint);
		}
	}

	RemoveFromParent();
}
