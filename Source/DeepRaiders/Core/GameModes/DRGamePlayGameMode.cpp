#include "DRGamePlayGameMode.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/DeepRaiders.h"

ADRGamePlayGameMode::ADRGamePlayGameMode()
{
	DefaultPawnClass = nullptr;
	HostOrJoinWidgetInstance = nullptr;
}

void ADRGamePlayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));

	if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer || GetNetMode() == NM_Client)
		return;

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

		FInputModeGameAndUI InputModeData;
		if (HostOrJoinWidgetInstance)
		{
			InputModeData.SetWidgetToFocus(HostOrJoinWidgetInstance->TakeWidget());
		}
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		NewPlayer->SetInputMode(InputModeData);
	}
}

void ADRGamePlayGameMode::BeginPlay()
{
	Super::BeginPlay();

	DR_LOG(TEXT("BeginPlay"));
}
