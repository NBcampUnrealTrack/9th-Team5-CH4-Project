#include "DRTitleGameMode.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/DeepRaiders.h"
#include "GameFramework/PlayerController.h"

ADRTitleGameMode::ADRTitleGameMode()
{
	DefaultPawnClass = nullptr;
	HostOrJoinWidgetInstance = nullptr;
}

void ADRTitleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));

	if (NewPlayer && NewPlayer->IsLocalController())
	{
		if (HostOrJoinWidgetClass)
		{
			HostOrJoinWidgetInstance = CreateWidget<UUserWidget>(NewPlayer, HostOrJoinWidgetClass);
			if (HostOrJoinWidgetInstance)
			{
				HostOrJoinWidgetInstance->AddToViewport();
			}
		}

		NewPlayer->SetShowMouseCursor(true);

		FInputModeUIOnly InputModeData;
		if (HostOrJoinWidgetInstance)
		{
			InputModeData.SetWidgetToFocus(HostOrJoinWidgetInstance->TakeWidget());
		}
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		NewPlayer->SetInputMode(InputModeData);
	}
}
