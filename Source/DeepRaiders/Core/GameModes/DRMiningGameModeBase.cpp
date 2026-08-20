#include "DRMiningGameModeBase.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"

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

	// =============================
	// TEMP: Team assignment
	// =============================

	if (ADRPlayerState* PlayerState = PlayerController->GetPlayerState<ADRPlayerState>())
	{
		const int32 AssignedTeamId = PlayerState->GetPlayerId() % 2;

		PlayerState->SetTeamId(AssignedTeamId);

		UE_LOG(LogTemp, Warning, TEXT( "[Team] Player=%s " "PlayerId=%d TeamId=%d"), *GetNameSafe(PlayerState), PlayerState->GetPlayerId(), AssignedTeamId);
	}

	// =============================
	// Existing terrain sync
	// =============================

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

	// DRPlayerController 리팩토링으로 인해 사용이 불가능합니다.
	//PlayerController->Client_ApplyTerrainDigHistory(DigHistory);
}
