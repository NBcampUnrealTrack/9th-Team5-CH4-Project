#include "DRTitleGameMode.h"

#include "DeepRaiders/DeepRaiders.h"

ADRTitleGameMode::ADRTitleGameMode()
{
}

void ADRTitleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));
}
