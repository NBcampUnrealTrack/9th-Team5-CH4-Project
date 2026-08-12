#include "DRMiningGameModeBase.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"


ADRMiningGameModeBase::ADRMiningGameModeBase()
{
	GameStateClass = ADRMiningGameStateBase::StaticClass();
}

void ADRMiningGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(NewPlayer);
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UDRVoxelTerrainSubsystem* TerrainSubsystem =
		World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(TerrainSubsystem))
	{
		return;
	}

	const TArray<FDRTerrainDigOperation>& DigHistory =
		TerrainSubsystem->GetDigHistory();
	if (DigHistory.Num() == 0)
	{
		return;
	}

	// 중도난입한 플레이어에게 서버가 확정한 지형 변경 이력을 한 번에 전달한다.
	PlayerController->QueueTerrainDigHistory(DigHistory);
}
