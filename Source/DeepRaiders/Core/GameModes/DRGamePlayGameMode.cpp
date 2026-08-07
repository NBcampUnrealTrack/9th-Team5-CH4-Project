#include "DRGamePlayGameMode.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/DeepRaiders.h"
#include "DeepRaiders/Shop/DRShopTestPlayerState.h"

ADRGamePlayGameMode::ADRGamePlayGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerStateClass = ADRShopTestPlayerState::StaticClass();
	HostOrJoinWidgetInstance = nullptr;
}

void ADRGamePlayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));

	// 이미 서버에 연결된 경우 제외
	if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer || GetNetMode() == NM_Client)
		return;

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
