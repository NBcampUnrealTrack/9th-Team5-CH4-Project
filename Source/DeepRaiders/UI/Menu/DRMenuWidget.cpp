#include "DRMenuWidget.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Title/DRTitleSettingsWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UDRMenuWidget::HandleSettingsClicked()
{
	if (IsValid(WBP_Settings))
	{
		WBP_Settings->Show();
	}
}

void UDRMenuWidget::HandleMoveToTitleClicked()
{
	UGameplayStatics::OpenLevel(this, TitleMapName);
}

void UDRMenuWidget::HandleCloseClicked()
{
	APlayerController* PlayerController = GetOwningPlayer();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (UDRUIManagerSubsystem* UIManager = LocalPlayer
		? LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>()
		: nullptr)
	{
		UIManager->PopScreen(DRGameplayTags::UI_Screen_Menu);
	}
}
