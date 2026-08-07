#include "DRTitleGameMode.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/DeepRaiders.h"
#include "GameFramework/PlayerController.h"

ADRTitleGameMode::ADRTitleGameMode()
{
}

void ADRTitleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));
}
