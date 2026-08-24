#include "DRMiningGameModeBase.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/DRTeamPlayerStart.h"
#include "EngineUtils.h"

ADRMiningGameModeBase::ADRMiningGameModeBase()
{
	GameStateClass = ADRMiningGameStateBase::StaticClass();
}

void ADRMiningGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// 현재는 맵 시작과 동시에 지급하며, 추후 실제 경기 시작 지점으로 이동할 수 있다.
	StartTimer();
}

void ADRMiningGameModeBase::StartTimer()
{
	if (GetWorldTimerManager().IsTimerActive(PassiveCoinTimerHandle))
	{
		return;
	}

	PassiveCoinStartTime = GetWorld()->GetTimeSeconds();
	LastProcessedGrantIndex = 0;

	if (PassiveCoinInterval <= 0.f || PassiveCoinAmount <= 0)
	{
		return;
	}

	FTimerManagerTimerParameters TimerParameters;
	TimerParameters.bLoop = true;

	// 서버 지연 시 같은 프레임에서 타이머 콜백이 반복 실행되는 것을 방지한다.
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
	EndTimer();

	Super::EndPlay(EndPlayReason);
}

void ADRMiningGameModeBase::EndTimer()
{
	GetWorldTimerManager().ClearTimer(PassiveCoinTimerHandle);
}

void ADRMiningGameModeBase::GrantPassiveCoins()
{
	if (!IsValid(GameState))
	{
		return;
	}

	const double ElapsedTime = GetWorld()->GetTimeSeconds() - PassiveCoinStartTime;

	// 타이머 호출 횟수가 아닌 서버 경기 시간으로 지급 회차를 결정한다.
	const int64 CurrentGrantIndex = FMath::FloorToInt64(ElapsedTime / PassiveCoinInterval);

	if (CurrentGrantIndex <= LastProcessedGrantIndex)
	{
		return;
	}

	int64 TotalGrantAmount = 0;

	// 서버 지연 중 누락된 회차는 당시 지급량으로 계산한 뒤 한 번에 지급한다.
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

	// 다음 호출에서 같은 회차가 다시 지급되지 않도록 먼저 처리 위치를 갱신한다.
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

	// 해당 지급 회차가 몇 번째 지급량 증가 구간에 속하는지 계산한다.
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
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(NewPlayer);

	// 첫 스폰 위치를 선택하기 전에 서버에서 팀을 확정한다.
	if (IsValid(PlayerController))
	{
		if (ADRPlayerState* PlayerState = PlayerController->GetPlayerState<ADRPlayerState>())
		{
			const int32 AssignedTeamId = PlayerState->GetPlayerId() % 2;

			PlayerState->SetTeamId(AssignedTeamId);

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[Team] Player=%s PlayerId=%d TeamId=%d"),
				*GetNameSafe(PlayerState),
				PlayerState->GetPlayerId(),
				AssignedTeamId);
		}
	}

	Super::PostLogin(NewPlayer);

	if (!IsValid(PlayerController))
	{
		return;
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

AActor* ADRMiningGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	const ADRPlayerState* PlayerState =
		IsValid(Player) ? Player->GetPlayerState<ADRPlayerState>() : nullptr;
	if (!IsValid(PlayerState))
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	TArray<ADRTeamPlayerStart*> TeamStarts;
	for (TActorIterator<ADRTeamPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (Iterator->TeamId == PlayerState->GetTeamId())
		{
			TeamStarts.Add(*Iterator);
		}
	}

	if (TeamStarts.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[TeamSpawn] TeamId=%d 시작점이 없어 일반 PlayerStart를 사용합니다."),
			PlayerState->GetTeamId());
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	return TeamStarts[FMath::RandHelper(TeamStarts.Num())];
}
