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

void ADRMiningGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	PassiveCoinStartTime = GetWorld()->GetTimeSeconds();
	LastProcessedGrantIndex = 0;

	if (PassiveCoinInterval <= 0.f || PassiveCoinAmount <= 0)
	{
		return;
	}

	FTimerManagerTimerParameters TimerParameters;
	TimerParameters.bLoop = true;
	TimerParameters.bMaxOncePerFrame = true;

	GetWorldTimerManager().SetTimer(
		PassiveCoinTimerHandle,
		this,
		&ThisClass::GrantPassiveCoins,
		PassiveCoinInterval,
		TimerParameters);
}

void ADRMiningGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PassiveCoinTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void ADRMiningGameModeBase::GrantPassiveCoins()
{
	if (!IsValid(GameState))
	{
		return;
	}

	const double ElapsedTime = GetWorld()->GetTimeSeconds() - PassiveCoinStartTime;
	const int64 CurrentGrantIndex = FMath::FloorToInt64(ElapsedTime / PassiveCoinInterval);

	if (CurrentGrantIndex <= LastProcessedGrantIndex)
	{
		return;
	}

	int64 TotalGrantAmount = 0;

	for (int64 GrantIndex = LastProcessedGrantIndex + 1; GrantIndex <= CurrentGrantIndex; ++GrantIndex)
	{
		TotalGrantAmount = FMath::Min<int64>(
			MAX_int32,
			TotalGrantAmount + GetPassiveCoinAmountAtGrantIndex(GrantIndex));

		if (TotalGrantAmount == MAX_int32)
		{
			break;
		}
	}

	LastProcessedGrantIndex = CurrentGrantIndex;
	const int32 GrantAmount = static_cast<int32>(TotalGrantAmount);

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ADRPlayerState* DRPlayerState = Cast<ADRPlayerState>(PlayerState);

		if (IsValid(DRPlayerState))
		{
			DRPlayerState->AddCoins(GrantAmount);
		}
	}
}

int64 ADRMiningGameModeBase::GetPassiveCoinAmountAtGrantIndex(int64 GrantIndex) const
{
	if (PassiveCoinIncreaseInterval <= 0.f || PassiveCoinIncreaseAmount <= 0)
	{
		return PassiveCoinAmount;
	}

	const double GrantElapsedTime = GrantIndex * static_cast<double>(PassiveCoinInterval);
	const int64 IncreaseStep = FMath::FloorToInt64(GrantElapsedTime / PassiveCoinIncreaseInterval);
	const int64 MaxIncreaseStep = (MAX_int32 - PassiveCoinAmount) / PassiveCoinIncreaseAmount;

	if (IncreaseStep > MaxIncreaseStep)
	{
		return MAX_int32;
	}

	return PassiveCoinAmount + IncreaseStep * PassiveCoinIncreaseAmount;
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
