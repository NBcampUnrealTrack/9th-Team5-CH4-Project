#include "DRGamePlayGameMode.h"

#include "DeepRaiders/DeepRaiders.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"

ADRGamePlayGameMode::ADRGamePlayGameMode()
{
}

void ADRGamePlayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	DR_LOG(TEXT("PostLogin"));

	Super::PostLogin(NewPlayer);

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(NewPlayer);
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UDRVoxelTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(TerrainSubsystem))
	{
		return;
	}

	const TArray<FDRTerrainDigOperation>& DigHistory = TerrainSubsystem->GetDigHistory();
	if (DigHistory.Num() == 0)
	{
		return;
	}

	// 중도난입한 플레이어에게 서버가 확정한 지형 변경 이력을 한 번에 전달한다.
	PlayerController->Client_ApplyTerrainDigHistory(DigHistory);
}

void ADRGamePlayGameMode::BeginPlay()
{
	Super::BeginPlay();

	DR_LOG(TEXT("BeginPlay"));
}
