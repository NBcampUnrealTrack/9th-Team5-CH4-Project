#include "DRUIManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "GameFramework/PlayerController.h"

void UDRUIManagerSubsystem::Deinitialize()
{
	for (UUserWidget* Widget : ManagedWidgets)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
	}

	ManagedWidgets.Reset();
	WidgetLayers.Reset();
	PlayerController = nullptr;
	UIConfig = nullptr;
	Super::Deinitialize();
}

void UDRUIManagerSubsystem::Configure(
	APlayerController* InPlayerController,
	UDRUIConfig* InUIConfig)
{
	PlayerController = InPlayerController;
	UIConfig = InUIConfig;
}

UUserWidget* UDRUIManagerSubsystem::CreateManagedWidget(
	TSubclassOf<UUserWidget> WidgetClass,
	EDRUILayer Layer)
{
	if (!IsValid(PlayerController) || !WidgetClass)
	{
		return nullptr;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	if (IsValid(Widget))
	{
		ManagedWidgets.AddUnique(Widget);
		WidgetLayers.Add(TWeakObjectPtr<UUserWidget>(Widget), Layer);
		Widget->AddToViewport(GetLayerZOrder(Layer));
		RefreshInputMode();
	}

	return Widget;
}

void UDRUIManagerSubsystem::SetManagedWidgetVisible(UUserWidget* Widget, bool bVisible)
{
	if (!IsValid(Widget)
		|| !WidgetLayers.Contains(TWeakObjectPtr<UUserWidget>(Widget)))
	{
		return;
	}

	Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RefreshInputMode();
}

void UDRUIManagerSubsystem::ReleaseManagedWidget(UUserWidget* Widget)
{
	if (IsValid(Widget))
	{
		Widget->RemoveFromParent();
	}

	ManagedWidgets.Remove(Widget);
	WidgetLayers.Remove(TWeakObjectPtr<UUserWidget>(Widget));
	RefreshInputMode();
}

void UDRUIManagerSubsystem::RefreshInputMode()
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	UUserWidget* ActiveModal = nullptr;
	bool bHasActiveMenu = false;

	for (UUserWidget* Widget : ManagedWidgets)
	{
		if (!IsValid(Widget) || Widget->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}

		const EDRUILayer* Layer = WidgetLayers.Find(TWeakObjectPtr<UUserWidget>(Widget));
		if (!Layer)
		{
			continue;
		}

		if (*Layer == EDRUILayer::Modal)
		{
			ActiveModal = Widget;
		}
		else if (*Layer == EDRUILayer::Menu)
		{
			bHasActiveMenu = true;
		}
	}

	if (IsValid(ActiveModal))
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveModal->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
		return;
	}

	if (bHasActiveMenu)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
		return;
	}

	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->bShowMouseCursor = false;
}

int32 UDRUIManagerSubsystem::GetLayerZOrder(EDRUILayer Layer)
{
	switch (Layer)
	{
	case EDRUILayer::HUD:
		return 0;
	case EDRUILayer::Menu:
		return 100;
	case EDRUILayer::Modal:
		return 200;
	default:
		return 0;
	}
}
