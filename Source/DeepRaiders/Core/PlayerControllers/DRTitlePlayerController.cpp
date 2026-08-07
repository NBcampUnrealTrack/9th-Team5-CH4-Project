#include "DRTitlePlayerController.h"

#include "Blueprint/UserWidget.h"

void ADRTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 호스트 또는 접속을 위한 위젯을 생성 하는 부분
	if (!IsLocalController())
		return;

	if (HostOrJoinWidgetClass)
	{
		HostOrJoinWidgetInstance = CreateWidget<UUserWidget>(this, HostOrJoinWidgetClass);
		if (HostOrJoinWidgetInstance)
		{
			HostOrJoinWidgetInstance->AddToViewport();
		}
	}

	SetShowMouseCursor(true);

	FInputModeUIOnly InputModeData;
	SetInputMode(InputModeData);
}
