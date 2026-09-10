#include "HostOrJoinWidget.h"

#include "DRTitleJoinWidget.h"
#include "DRTitleMapChoiceWidget.h"
#include "DRTitleSettingsWidget.h"
#include "DeepRaiders/UI/RoomService/DRRoomServiceWidget.h"
#include "DeepRaiders/UI/RoomService/DRCreateRoomWidget.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"

void UHostOrJoinWidget::NativeConstruct()
{
	Super::NativeConstruct();
	WBP_RoomService->SetVisibility(ESlateVisibility::Collapsed);
	WBP_CreateRoom->SetVisibility(ESlateVisibility::Collapsed);
}

void UHostOrJoinWidget::HandlePublicMatchClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[RoomUI] Title room button clicked. RoomService=%s"),
		*GetNameSafe(WBP_RoomService));
	if (!WBP_RoomService)
	{
		return;
	}
	// Title에 배치된 인스턴스를 재사용하며 부모 루트는 숨기지 않는다.
	WBP_RoomService->Open(this, Overlay_Title, WBP_CreateRoom, WBP_Join);
}

void UHostOrJoinWidget::HandlePrivateCreateClicked()
{
	if (WBP_ChoiceMap)
	{
		WBP_ChoiceMap->Show();
	}
}

void UHostOrJoinWidget::HandlePrivateMatchClicked()
{
	WBP_Join->Show();
}

void UHostOrJoinWidget::HandleSettingsClicked()
{
	WBP_Settings->Show();
}

void UHostOrJoinWidget::HandleExitGameClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UHostOrJoinWidget::HandleCreateMapClicked()
{
	if (WBP_ChoiceMap)
	{
		WBP_ChoiceMap->HandleCreateMapClicked();
	}
}

void UHostOrJoinWidget::HandleCloseChoiceMapClicked()
{
	if (WBP_ChoiceMap)
	{
		WBP_ChoiceMap->HandleCloseChoiceMapClicked();
	}
}

void UHostOrJoinWidget::HandleJoinClicked()
{
	WBP_Join->HandleJoinClicked();
}

void UHostOrJoinWidget::HandleCloseJoinClicked()
{
	WBP_Join->HandleCloseJoinClicked();
}

void UHostOrJoinWidget::HandleSettingsApplyClicked()
{
	WBP_Settings->HandleSettingsApplyClicked();
}

void UHostOrJoinWidget::HandleSettingsCancelClicked()
{
	WBP_Settings->HandleSettingsCancelClicked();
}
