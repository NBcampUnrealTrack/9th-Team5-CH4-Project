#include "HostOrJoinWidget.h"

#include "DRTitleJoinWidget.h"
#include "DRTitleMapChoiceWidget.h"
#include "DRTitleSettingsWidget.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"

void UHostOrJoinWidget::HandlePublicMatchClicked()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
		{
			SessionSubsystem->JoinDedicatedServer(DedicatedServerAddress);
		}
	}
}

void UHostOrJoinWidget::HandlePrivateCreateClicked()
{
	WBP_ChoiceMap->Show();
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
	WBP_ChoiceMap->HandleCreateMapClicked();
}

void UHostOrJoinWidget::HandleCloseChoiceMapClicked()
{
	WBP_ChoiceMap->HandleCloseChoiceMapClicked();
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
