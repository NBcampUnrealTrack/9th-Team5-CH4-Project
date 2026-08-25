#include "DRMiningGameModeBase.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/DRTeamPlayerStart.h"
#include "DeepRaiders/Gameplay/Team/DRTeamMovingActor.h"
#include "DeepRaiders/Snow/DRSnowControlZone.h"
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
	StartTeamSwitchTimer();
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
	GetWorldTimerManager().ClearTimer(TeamSwitchTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void ADRMiningGameModeBase::StartTeamSwitchTimer()
{
	RefreshActiveTeam();
	ApplyActiveTeam(true);

	if (TeamSwitchInterval <= 0.f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		TeamSwitchTimerHandle,
		this,
		&ThisClass::RefreshActiveTeam,
		TeamSwitchInterval,
		true);
}

void ADRMiningGameModeBase::RefreshActiveTeam()
{
	float TeamAmounts[2] = {0.f, 0.f};

	// 모든 거점의 눈 양을 합산해 현재 열세 팀을 결정한다.
	for (TActorIterator<ADRSnowControlZone> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		for (const FDRSnowTeamAmount& Team : Iterator->GetControlRatio().Teams)
		{
			if (Team.TeamId == 0 || Team.TeamId == 1)
			{
				TeamAmounts[Team.TeamId] += Team.Amount;
			}
		}
	}

	if (FMath::IsNearlyEqual(TeamAmounts[0], TeamAmounts[1]))
	{
		ActiveTeamId = INDEX_NONE;
	}
	else
	{
		ActiveTeamId = TeamAmounts[0] < TeamAmounts[1] ? 0 : 1;
	}

	ApplyActiveTeam(false);
}

void ADRMiningGameModeBase::ApplyActiveTeam(bool bImmediate)
{
	for (TActorIterator<ADRTeamMovingActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->SetTeamActive(Iterator->GetTeamId() == ActiveTeamId, bImmediate);
	}
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

	if (IsValid(PlayerController))
	{
		if (ADRPlayerState* PlayerState = PlayerController->GetPlayerState<ADRPlayerState>())
		{
			const int32 AssignedTeamId = AssignBalancedTeam(PlayerState);

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

void ADRMiningGameModeBase::Logout(AController* Exiting)
{
	const ADRPlayerState* PlayerState =
		IsValid(Exiting) ? Exiting->GetPlayerState<ADRPlayerState>() : nullptr;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Logout] Player=%s PlayerId=%d TeamId=%d"),
		*GetNameSafe(PlayerState),
		IsValid(PlayerState) ? PlayerState->GetPlayerId() : INDEX_NONE,
		IsValid(PlayerState) ? PlayerState->GetTeamId() : INDEX_NONE);

	// 기본 Logout이 Pawn, Controller, PlayerState와 GameState PlayerArray를 정리한다.
	Super::Logout(Exiting);
}

int32 ADRMiningGameModeBase::AssignBalancedTeam(ADRPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState) || PlayerState->HasAssignedTeam())
	{
		return IsValid(PlayerState) ? PlayerState->GetTeamId() : INDEX_NONE;
	}

	int32 TeamCounts[2] = {0, 0};
	if (IsValid(GameState))
	{
		for (APlayerState* ExistingState : GameState->PlayerArray)
		{
			const ADRPlayerState* ExistingDRState = Cast<ADRPlayerState>(ExistingState);
			if (!IsValid(ExistingDRState) || ExistingDRState == PlayerState ||
				!ExistingDRState->HasAssignedTeam())
			{
				continue;
			}

			const int32 ExistingTeamId = ExistingDRState->GetTeamId();
			if (ExistingTeamId == 0 || ExistingTeamId == 1)
			{
				++TeamCounts[ExistingTeamId];
			}
		}
	}

	const int32 AssignedTeamId = TeamCounts[0] == TeamCounts[1]
		? FMath::RandRange(0, 1)
		: (TeamCounts[0] < TeamCounts[1] ? 0 : 1);
	PlayerState->SetTeamId(AssignedTeamId);
	return AssignedTeamId;
}

AActor* ADRMiningGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	ADRPlayerState* PlayerState =
		IsValid(Player) ? Player->GetPlayerState<ADRPlayerState>() : nullptr;
	if (!IsValid(PlayerState))
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}


	// ChoosePlayerStart가 PostLogin보다 먼저 호출될 수 있으므로 스폰 선택 전에 팀을 확정한다.
	AssignBalancedTeam(PlayerState);

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

	// 같은 팀 시작점이 여러 개여도 실행할 때마다 위치가 바뀌지 않도록 고정 순서로 선택한다.
	TeamStarts.Sort([](const ADRTeamPlayerStart& Left, const ADRTeamPlayerStart& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});

	return TeamStarts[0];
}
