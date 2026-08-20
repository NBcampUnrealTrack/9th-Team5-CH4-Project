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
	ActiveScreens.Reset();
	PlayerController = nullptr;
	UIConfig = nullptr;
	Super::Deinitialize();
}

UUserWidget* UDRUIManagerSubsystem::PushScreen(FGameplayTag ScreenTag)
{
	if (!ScreenTag.IsValid() || !IsValid(UIConfig))
	{
		return nullptr;
	}

	if (UUserWidget* ExistingWidget = GetScreen(ScreenTag))
	{
		SetManagedWidgetVisible(ExistingWidget, true);
		return ExistingWidget;
	}

	const FDRUIScreenDefinition* Definition = UIConfig->FindScreen(ScreenTag);
	if (!Definition || !Definition->WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UI screen is not configured: %s"),
			*ScreenTag.ToString());
		return nullptr;
	}

	UUserWidget* Widget = CreateManagedWidget(Definition->WidgetClass, Definition->Layer);
	if (IsValid(Widget))
	{
		ActiveScreens.Add(ScreenTag, Widget);
	}

	return Widget;
}

void UDRUIManagerSubsystem::PopScreen(FGameplayTag ScreenTag)
{
	if (UUserWidget* Widget = GetScreen(ScreenTag))
	{
		ReleaseManagedWidget(Widget);
	}
}

bool UDRUIManagerSubsystem::IsScreenOpen(FGameplayTag ScreenTag) const
{
	const UUserWidget* Widget = GetScreen(ScreenTag);
	return IsValid(Widget) && Widget->GetVisibility() != ESlateVisibility::Collapsed;
}

UUserWidget* UDRUIManagerSubsystem::GetScreen(FGameplayTag ScreenTag) const
{
	const TObjectPtr<UUserWidget>* Widget = ActiveScreens.Find(ScreenTag);
	return Widget && IsValid(Widget->Get()) ? Widget->Get() : nullptr;
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

	for (auto It = ActiveScreens.CreateIterator(); It; ++It)
	{
		if (It.Value() == Widget)
		{
			It.RemoveCurrent();
		}
	}

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
	case EDRUILayer::VFX:
		return 50;
	case EDRUILayer::Menu:
		return 100;
	case EDRUILayer::Modal:
		return 200;
	default:
		return 0;
	}
}
