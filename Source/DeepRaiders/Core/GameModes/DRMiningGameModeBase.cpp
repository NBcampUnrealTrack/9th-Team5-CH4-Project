#include "DRMiningGameModeBase.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"


ADRMiningGameModeBase::ADRMiningGameModeBase()
{
	GameStateClass = ADRMiningGameStateBase::StaticClass();
}

void ADRMiningGameModeBase::PostLogin(APlayerController* NewPlayer)
{
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

	ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>();
	UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>();
	if (IsValid(MiningGameState) && IsValid(SnowSubsystem))
	{
		FDRSnowJoinCheckpoint Checkpoint;
		if (!SnowSubsystem->GetLatestCheckpoint(Checkpoint) &&
			SnowSubsystem->CreateCheckpoint(MiningGameState->GetSnowOperationSequence()))
		{
			SnowSubsystem->GetLatestCheckpoint(Checkpoint);
			MiningGameState->DiscardSnowOperationsThrough(Checkpoint.OperationSequence);
		}

		if (SnowSubsystem->GetLatestCheckpoint(Checkpoint))
		{
			PlayerController->Client_BeginSnowJoinSnapshot(
				Checkpoint.SnapshotId,
				Checkpoint.OperationSequence,
				Checkpoint.VoxelWorldName,
				Checkpoint.VoxelSaveData.Num(),
				Checkpoint.SnowVolumeData.Num(),
				Checkpoint.OwnershipData.Num());
			return;
		}
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
