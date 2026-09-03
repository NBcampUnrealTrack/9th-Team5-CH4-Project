#include "DRUIManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/SViewport.h"

// UIOnly 입력도 매니저에 전달한다. 실제 처리 여부는 로컬 플레이어 포커스로 제한한다.
class FDREscapeInputProcessor : public IInputProcessor
{
public:
	explicit FDREscapeInputProcessor(UDRUIManagerSubsystem* InManager) : Manager(InManager)
	{
	}

	virtual void Tick(
		const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
	{
		if (KeyEvent.GetKey() != EKeys::Escape)
		{
			return false;
		}
		if (KeyEvent.IsRepeat())
		{
			return EscapeUserIndex == KeyEvent.GetUserIndex();
		}
		if (Manager.IsValid() && Manager->HandleEscapeKey(KeyEvent))
		{
			EscapeUserIndex = KeyEvent.GetUserIndex();
			return true;
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(
		FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
	{
		if (KeyEvent.GetKey() != EKeys::Escape || EscapeUserIndex != KeyEvent.GetUserIndex())
		{
			return false;
		}
		EscapeUserIndex = INDEX_NONE;
		return true;
	}

private:
	TWeakObjectPtr<UDRUIManagerSubsystem> Manager;
	int32 EscapeUserIndex = INDEX_NONE;
};

void UDRUIManagerSubsystem::Deinitialize()
{
	if (EscapeInputProcessor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(EscapeInputProcessor);
	}
	EscapeInputProcessor.Reset();
	ClearManagedWidgets();
	PlayerController = nullptr;
	UIConfig = nullptr;
	Super::Deinitialize();
}

void UDRUIManagerSubsystem::ClearManagedWidgets()
{
	CloseTargets.Reset();
	for (UUserWidget* Widget : ManagedWidgets)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
	}

	ManagedWidgets.Reset();
	WidgetLayers.Reset();
	WidgetZOrders.Reset();
	ActiveScreens.Reset();
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

	UUserWidget* Widget = CreateManagedWidget(
		Definition->WidgetClass,
		Definition->Layer,
		Definition->Order);
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

UUserWidget* UDRUIManagerSubsystem::GetTopScreen() const
{
	UUserWidget* TopScreen = nullptr;
	int32 TopZOrder = MIN_int32;
	for (UUserWidget* Widget : ManagedWidgets)
	{
		if (!IsValid(Widget) || !Widget->IsInViewport() || !Widget->IsVisible())
		{
			continue;
		}

		const EDRUILayer* Layer = WidgetLayers.Find(Widget);
		const int32* ZOrder = WidgetZOrders.Find(Widget);
		if (Layer && ZOrder && (*Layer == EDRUILayer::Menu || *Layer == EDRUILayer::Modal)
			&& *ZOrder >= TopZOrder)
		{
			TopScreen = Widget;
			TopZOrder = *ZOrder;
		}
	}
	return TopScreen;
}

void UDRUIManagerSubsystem::RegisterCloseHandler(UUserWidget* Widget, FSimpleDelegate Handler)
{
	if (!IsValid(Widget) || !Handler.IsBound())
	{
		return;
	}
	UnregisterCloseHandler(Widget);
	CloseTargets.Add({Widget, MoveTemp(Handler)});
}

void UDRUIManagerSubsystem::UnregisterCloseHandler(UUserWidget* Widget)
{
	const int32 RemovedCount = CloseTargets.RemoveAll([Widget](const FCloseTarget& Target)
	{
		return !Target.Widget.IsValid() || Target.Widget.Get() == Widget;
	});
	if (RemovedCount > 0)
	{
		RefreshInputMode();
	}
}

bool UDRUIManagerSubsystem::PopTopScreen()
{
	UUserWidget* Screen = GetTopScreen();
	if (!Screen)
	{
		return false;
	}

	// 최상단 화면에 속한 마지막 팝업부터 닫는다. 다른 화면의 팝업은 건드리지 않는다.
	for (int32 Index = CloseTargets.Num() - 1; Index >= 0; --Index)
	{
		UUserWidget* Target = CloseTargets[Index].Widget.Get();
		if (IsValid(Target) && Target->IsVisible() && (Target == Screen || Target->IsIn(Screen))
			&& CloseTargets[Index].Handler.IsBound())
		{
			// 콜백이 등록 목록 자체를 수정할 수 있으므로 복사해서 실행한다.
			FSimpleDelegate Handler = CloseTargets[Index].Handler;
			Handler.Execute();
			RefreshInputMode();
			return true;
		}
	}

	ReleaseManagedWidget(Screen);
	return true;
}

bool UDRUIManagerSubsystem::HandleEscapeKey(const FKeyEvent& KeyEvent)
{
	UUserWidget* Screen = GetTopScreen();
	if (KeyEvent.GetKey() != EKeys::Escape || !Screen || !IsValid(PlayerController))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
	if (!SlateUser || SlateUser->GetUserIndex() != KeyEvent.GetUserIndex())
	{
		return false;
	}

	const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient;
	const TSharedPtr<SViewport> Viewport = ViewportClient
		? ViewportClient->GetGameViewportWidget() : nullptr;
	const bool bViewportFocused = Viewport
		&& Viewport->HasUserFocus(KeyEvent.GetUserIndex()).IsSet();
	if (!Screen->HasUserFocus(PlayerController)
		&& !Screen->HasUserFocusedDescendants(PlayerController) && !bViewportFocused)
	{
		return false;
	}

	return KeyEvent.IsRepeat() || PopTopScreen();
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
	if (PlayerController != InPlayerController)
	{
		// LocalPlayerSubsystem은 맵 전환 후에도 남으므로 이전 월드의 UI를 정리한다.
		ClearManagedWidgets();
	}

	PlayerController = InPlayerController;
	UIConfig = InUIConfig;
	if (!EscapeInputProcessor && FSlateApplication::IsInitialized())
	{
		EscapeInputProcessor = MakeShared<FDREscapeInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(EscapeInputProcessor);
	}
}

UUserWidget* UDRUIManagerSubsystem::CreateManagedWidget(
	TSubclassOf<UUserWidget> WidgetClass,
	EDRUILayer Layer,
	int32 Order)
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
		WidgetZOrders.Add(Widget, GetLayerZOrder(Layer) + Order);
		if (Layer == EDRUILayer::Menu || Layer == EDRUILayer::Modal)
		{
			Widget->SetIsFocusable(true);
		}
		Widget->AddToViewport(GetLayerZOrder(Layer) + Order);
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

void UDRUIManagerSubsystem::SetManagedWidgetVisibilityOnly(UUserWidget* Widget, bool bVisible)
{
	if (!IsValid(Widget)
		|| !WidgetLayers.Contains(TWeakObjectPtr<UUserWidget>(Widget)))
	{
		return;
	}

	Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UDRUIManagerSubsystem::ReleaseManagedWidget(UUserWidget* Widget)
{
	if (IsValid(Widget))
	{
		Widget->RemoveFromParent();
	}

	CloseTargets.RemoveAll([Widget](const FCloseTarget& Target)
	{
		UUserWidget* TargetWidget = Target.Widget.Get();
		return !TargetWidget || TargetWidget == Widget || (Widget && TargetWidget->IsIn(Widget));
	});
	ManagedWidgets.Remove(Widget);
	WidgetLayers.Remove(TWeakObjectPtr<UUserWidget>(Widget));
	WidgetZOrders.Remove(Widget);

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

	UUserWidget* TopScreen = GetTopScreen();
	const EDRUILayer* Layer = WidgetLayers.Find(TopScreen);
	UUserWidget* ActiveModal = Layer && *Layer == EDRUILayer::Modal ? TopScreen : nullptr;
	UUserWidget* ActiveMenu = Layer && *Layer == EDRUILayer::Menu ? TopScreen : nullptr;

	if (IsValid(ActiveModal))
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveModal->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
		return;
	}

	if (IsValid(ActiveMenu))
	{
		// 메뉴 조작 중에는 게임 입력을 막고 UI 입력만 받는다.
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveMenu->TakeWidget());
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
