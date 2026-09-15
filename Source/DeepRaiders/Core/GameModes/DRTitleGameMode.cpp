#include "DRTitleGameMode.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/DeepRaiders.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
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

	// 호스트 또는 접속을 위한 위젯을 생성 하는 부분
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
			HostOrJoinWidgetInstance->SetIsFocusable(true);
			InputModeData.SetWidgetToFocus(HostOrJoinWidgetInstance->TakeWidget());
		}
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		NewPlayer->SetInputMode(InputModeData);
	}
}

void ADRTitleGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (UDRSessionSubsystem* SessionSubsystem = GetGameInstance()->GetSubsystem<UDRSessionSubsystem>())
	{
		SessionSubsystem->CreateDedicatedServerSession();
	}
}
