#include "DRMiningGameModeBase.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/DRTeamPlayerStart.h"
#include "DeepRaiders/Gameplay/Team/DRTeamMovingActor.h"
#include "DeepRaiders/Gameplay/DRGameStartActor.h"
#include "DeepRaiders/Snow/DRSnowControlZone.h"
#include "GameFramework/PawnMovementComponent.h"
#include "EngineUtils.h"
#include "VoxelWorld.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"

ADRMiningGameModeBase::ADRMiningGameModeBase()
{
	GameStateClass = ADRMiningGameStateBase::StaticClass();
	bStartPlayersAsSpectators = true;
}

void ADRMiningGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	bIsGameStart = false;
	bIsGameEnd = false;
}

bool ADRMiningGameModeBase::ShouldSpawnAtStartSpot(AController*)
{
	// 팀 변경을 반영하기 위해 최초 접속 시 저장된 StartSpot을 재사용하지 않는다.
	return false;
}

bool ADRMiningGameModeBase::StartGame()
{
	if (!HasAuthority() || bIsGameStart)
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(GameResultTimerHandle);
	ResetGameState();
	bIsGameStart = true;
	bIsGameEnd = false;
	StartTeamSwitchTimer();
	GameRemainingSeconds = FMath::Max(1, FMath::CeilToInt(GameDuration));
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameEndDebugText(FString());
		MiningGameState->SetGameResultText(FText::GetEmpty());
		MiningGameState->SetGameTimerState(GameRemainingSeconds, true, false);
	}
	GetWorldTimerManager().SetTimer(
		GameTimerHandle,
		this,
		&ThisClass::TickGameTimer,
		1.f,
		true);
	return true;
}

void ADRMiningGameModeBase::TickGameTimer()
{
	if (!bIsGameStart || bIsGameEnd)
	{
		return;
	}

	--GameRemainingSeconds;
	if (GameRemainingSeconds <= 0)
	{
		EndGame();
		return;
	}

	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameTimerState(GameRemainingSeconds, true, false);
	}
}

void ADRMiningGameModeBase::EndGame()
{
	if (!HasAuthority() || !bIsGameStart || bIsGameEnd)
	{
		return;
	}

	bIsGameStart = false;
	bIsGameEnd = true;
	GetWorldTimerManager().ClearTimer(TeamSwitchTimerHandle);
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	GameRemainingSeconds = 0;
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameTimerState(0, false, true);
	}

	double WeightedTeamScores[2] = {0.0, 0.0};
	TArray<FString> ZoneDebugTexts;
	for (TActorIterator<ADRSnowControlZone> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const FDRSnowVoxelMaterialScanResult MaterialScan = Iterator->ScanVoxelMaterials();
		const FString ZoneDebugText = Iterator->BuildSnowCountDebugTextFromScan(MaterialScan);
		ZoneDebugTexts.Add(FString::Printf(TEXT("[%s]\n%s"), *Iterator->GetName(), *ZoneDebugText));
		UE_LOG(LogTemp, Warning, TEXT("[GameEnd][Zone=%s]\n%s"), *Iterator->GetName(), *ZoneDebugText);
		Iterator->RefreshControlRatio();
		const FDRSnowControlRatio Ratio = Iterator->GetControlRatio();
		float ZoneTeamAmounts[2] = {0.f, 0.f};
		for (const FDRSnowTeamAmount& Team : Ratio.Teams)
		{
			if (Team.TeamId == 0 || Team.TeamId == 1)
			{
				ZoneTeamAmounts[Team.TeamId] += Team.Amount;
			}
		}

		const float ZoneTeamTotal = ZoneTeamAmounts[0] + ZoneTeamAmounts[1];
		const float ZoneTeam0Percent =
			ZoneTeamTotal > 0.f ? ZoneTeamAmounts[0] / ZoneTeamTotal * 100.f : 0.f;
		const float ZoneTeam1Percent =
			ZoneTeamTotal > 0.f ? ZoneTeamAmounts[1] / ZoneTeamTotal * 100.f : 0.f;
		WeightedTeamScores[0] += ZoneTeam0Percent * Iterator->GetPointValue();
		WeightedTeamScores[1] += ZoneTeam1Percent * Iterator->GetPointValue();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GameEnd][Zone=%s] Point=%.2f Team 0=%.2f%% Team 1=%.2f%%"),
			*Iterator->GetName(),
			Iterator->GetPointValue(),
			ZoneTeam0Percent,
			ZoneTeam1Percent);
	}

	const double WeightedScoreTotal = WeightedTeamScores[0] + WeightedTeamScores[1];
	const int32 Team0Percent = WeightedScoreTotal > 0.0
		? FMath::RoundToInt(WeightedTeamScores[0] / WeightedScoreTotal * 100.0)
		: 0;
	const int32 Team1Percent = WeightedScoreTotal > 0.0 ? 100 - Team0Percent : 0;
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GameEnd] 게임 끝! Team 0=%d%% Team 1=%d%%"),
		Team0Percent,
		Team1Percent);

	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameEndDebugText(FString::Join(ZoneDebugTexts, TEXT("\n\n")));
		MiningGameState->SetGameResultText(FText::FromString(FString::Printf(
			TEXT("[Red] %d : %d [Blue]"),
			Team0Percent,
			Team1Percent)));
		GetWorldTimerManager().SetTimer(
			GameResultTimerHandle,
			this,
			&ThisClass::ClearGameResultText,
			GameResultDisplayDuration,
			false);
	}

	for (TActorIterator<ADRGameStartActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->ResetForNextGame();
	}
}

void ADRMiningGameModeBase::ClearGameResultText()
{
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameResultText(FText::GetEmpty());
		MiningGameState->SetGameTimerState(0, false, false);
	}
}

void ADRMiningGameModeBase::ResetGameState()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->Multicast_ResetVoxelState();
	}

	if (!IsValid(GameState))
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ADRPlayerState* DRPlayerState = Cast<ADRPlayerState>(PlayerState))
		{
			DRPlayerState->ResetForGameStart();
		}
	}

	for (
		FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Iterator->Get()))
		{
			PlayerController->ResetForGameStart();

			APawn* Pawn = PlayerController->GetPawn();
			if (!IsValid(Pawn))
			{
				RestartPlayer(PlayerController);
				continue;
			}

			// 준비 중 팀이 바뀔 수 있으므로 기존 StartSpot 캐시 대신 현재 팀으로 다시 선택한다.
			AActor* PlayerStart = ChoosePlayerStart(PlayerController);
			if (!IsValid(PlayerStart))
			{
				continue;
			}

			if (UPawnMovementComponent* MovementComponent = Pawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}

			Pawn->TeleportTo(
				PlayerStart->GetActorLocation(),
				PlayerStart->GetActorRotation(),
				false,
				true);
		}
	}
}

void ADRMiningGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TeamSwitchTimerHandle);
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	GetWorldTimerManager().ClearTimer(GameResultTimerHandle);

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

void ADRMiningGameModeBase::EnsureDevelopmentPlayerName(ADRPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return;
	}

#if WITH_EDITOR

	const FString CurrentName = PlayerState->GetPlayerName().TrimStartAndEnd();

	const bool bNeedsFallback = CurrentName.IsEmpty() || CurrentName.StartsWith(TEXT("DESKTOP-"), ESearchCase::IgnoreCase);

	if (!bNeedsFallback)
	{
		return;
	}

	const int32 PlayerId = PlayerState->GetPlayerId();

	const FString FallbackName = PlayerId > 0 ? FString::Printf(TEXT("Player %d"), PlayerId) : TEXT("Player");

	PlayerState->SetPlayerName(FallbackName);

	PlayerState->ForceNetUpdate();

#endif
}

void ADRMiningGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(NewPlayer);

	if (IsValid(PlayerController))
	{
		if (ADRPlayerState* PlayerState = PlayerController->GetPlayerState<ADRPlayerState>())
		{
			EnsureDevelopmentPlayerName(PlayerState);
			
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
	RefreshGameStartPlayerRoster();

	if (!IsValid(PlayerController))
	{
		return;
	}

	// 스냅샷 적용 전에는 Pawn을 생성하지 않고 관전 상태로 대기한다.
	PlayerController->ChangeState(NAME_Spectating);
	PlayerController->ClientGotoState(NAME_Spectating);

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (!TryStartSnowJoinSnapshot(PlayerController))
	{
		// 저장된 눈 상태가 없으면 대기하지 않고 바로 플레이를 시작한다.
		HandleSnowJoinSnapshotApplied(PlayerController, false);
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

bool ADRMiningGameModeBase::TryStartSnowJoinSnapshot(ADRPlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	ADRMiningGameStateBase* MiningGameState = IsValid(World)
		? World->GetGameState<ADRMiningGameStateBase>()
		: nullptr;
	UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(MiningGameState) || !IsValid(SnowSubsystem))
	{
		return false;
	}

	// 퇴적 요청을 먼저 막은 뒤 현재 VoxelWorld 상태로 중도 난입용 checkpoint를 새로 만든다.
	// 기존 checkpoint를 재사용하면 그 이후 DepositArea가 만든 복셀이 포함되지 않는다.
	OnJoinSnapshotStarted.Broadcast();
	if (!SnowSubsystem->CreateCheckpoint(MiningGameState->GetSnowOperationSequence()))
	{
		OnJoinSnapshotFinished.Broadcast(EDRSnowJoinSnapshotResult::InvalidCheckpoint);
		return false;
	}

	FDRSnowJoinCheckpoint Checkpoint;
	if (!SnowSubsystem->GetLatestCheckpoint(Checkpoint))
	{
		OnJoinSnapshotFinished.Broadcast(EDRSnowJoinSnapshotResult::InvalidCheckpoint);
		return false;
	}

	PlayerController->Client_BeginSnowJoinSnapshot(
		Checkpoint.SnapshotId,
		Checkpoint.OperationSequence,
		Checkpoint.VoxelWorldName,
		Checkpoint.VoxelSaveData.Num(),
		Checkpoint.SnowVolumeData.Num());
	return true;
}

bool ADRMiningGameModeBase::HandleSnowJoinSnapshotApplied(
	APlayerController* PlayerController,
	bool bNotifySnapshotFinished)
{
	bool bPlayerRestarted = false;
	if (IsValid(PlayerController) && !IsValid(PlayerController->GetPawn()))
	{
		PlayerController->ChangeState(NAME_Playing);
		PlayerController->ClientGotoState(NAME_Playing);
		RestartPlayer(PlayerController);

		if (APawn* SpawnedPawn = PlayerController->GetPawn(); IsValid(SpawnedPawn))
		{
			// Prioritize the initial Pawn and possession state before deposits are
			// allowed to resume and generate more replicated snow operations.
			SpawnedPawn->ForceNetUpdate();
			PlayerController->ForceNetUpdate();
			bPlayerRestarted = true;
		}
	}

	if (bNotifySnapshotFinished)
	{
		OnJoinSnapshotFinished.Broadcast(EDRSnowJoinSnapshotResult::Applied);
	}

	return bPlayerRestarted;
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
	RefreshGameStartPlayerRoster();
}

void ADRMiningGameModeBase::RefreshGameStartPlayerRoster()
{
	for (TActorIterator<ADRGameStartActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->RefreshPlayerRoster();
	}
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
