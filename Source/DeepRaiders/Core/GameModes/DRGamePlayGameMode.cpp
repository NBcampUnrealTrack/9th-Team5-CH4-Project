#include "DRGamePlayGameMode.h"

#include "DeepRaiders/DeepRaiders.h"

ADRGamePlayGameMode::ADRGamePlayGameMode()
{
}

void ADRGamePlayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));
}

void ADRGamePlayGameMode::BeginPlay()
{
	Super::BeginPlay();

	DR_LOG(TEXT("BeginPlay"));
}
