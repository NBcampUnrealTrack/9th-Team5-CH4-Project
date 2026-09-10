#include "DRTitleJoinWidget.h"

#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
#include "GameFramework/PlayerController.h"

bool UDRTitleJoinWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!IsDesignTime())
	{
		Overlay_Join->SetVisibility(ESlateVisibility::Collapsed);
	}

	return true;
}

void UDRTitleJoinWidget::Show(UUserWidget* InReturnWidget)
{
	ReturnWidget = InReturnWidget;
	if (ReturnWidget.IsValid())
	{
		ReturnWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::Visible);
	Overlay_Join->SetVisibility(ESlateVisibility::Visible);
	ETB_IPAddress->SetKeyboardFocus();
}

void UDRTitleJoinWidget::HandleJoinClicked()
{
	const FString Address = ETB_IPAddress->GetText().ToString().TrimStartAndEnd();
	if (Address.IsEmpty())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
		{
			SessionSubsystem->JoinListenServer(Address);
		}
	}
}

void UDRTitleJoinWidget::HandleCloseJoinClicked()
{
	Overlay_Join->SetVisibility(ESlateVisibility::Collapsed);
	// 전체 화면 접속 위젯이 뒤의 방 목록 입력을 가로채지 않게 한다.
	SetVisibility(ESlateVisibility::Collapsed);
	if (ReturnWidget.IsValid())
	{
		ReturnWidget->SetVisibility(ESlateVisibility::Visible);
		ReturnWidget->SetIsFocusable(true);
		if (APlayerController* Controller = GetOwningPlayer())
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(ReturnWidget->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
			Controller->SetShowMouseCursor(true);
		}
		ReturnWidget->SetKeyboardFocus();
		ReturnWidget.Reset();
	}
}
