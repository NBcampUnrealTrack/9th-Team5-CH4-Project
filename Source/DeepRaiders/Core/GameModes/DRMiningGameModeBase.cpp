#include "DRMiningGameModeBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/Components/DRSnowJoinComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/DRTeamPlayerStart.h"
#include "DeepRaiders/Gameplay/Team/DRTeamMovingActor.h"
#include "DeepRaiders/Gameplay/DRGameStartActor.h"
#include "DeepRaiders/Gameplay/Voxel/DRMeshVoxelCarver.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Snow/DRSnowControlZone.h"
#include "GameFramework/PawnMovementComponent.h"
#include "EngineUtils.h"
#include "VoxelWorld.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

FDRPhaseCountdownState FDRGamePhaseConfig::MakeCountdownState(int32 RemainingSeconds) const
{
	FDRPhaseCountdownState Countdown;
	if (RemainingSeconds <= 0)
	{
		return Countdown;
	}
	const int32 Elapsed = DurationSeconds - RemainingSeconds;
	// 표시 구간이 겹치면 종료 카운트다운을 먼저 선택한다.
	if (bShowExitCountdown && RemainingSeconds <= ExitCountdownSeconds)
	{
		Countdown.RemainingSeconds = RemainingSeconds;
		Countdown.bIsExitCountdown = true;
		Countdown.Text = ExitCountdownText;
	}
	else if (bShowEntryCountdown && Elapsed < EntryCountdownSeconds)
	{
		Countdown.RemainingSeconds = FMath::Min(EntryCountdownSeconds - Elapsed, RemainingSeconds);
		Countdown.Text = EntryCountdownText;
	}
	return Countdown;
}

ADRMiningGameModeBase::ADRMiningGameModeBase()
{
	GameStateClass = ADRMiningGameStateBase::StaticClass();
	bStartPlayersAsSpectators = true;
	// 기본 준비 페이즈는 20초씩 세 번, 60초 이후에는 거점전을 진행한다.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FDRGamePhaseConfig& Phase = GamePhases.AddDefaulted_GetRef();
		Phase.PhaseIndex = Index;
		Phase.DurationSeconds = Index < 3 ? 20 : 120;
	}
	RecalculateGameDuration();
}

void ADRMiningGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	RecalculateGameDuration();
}

#if WITH_EDITOR
void ADRMiningGameModeBase::PostEditChangeChainProperty(
	FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	RecalculateGameDuration();
}
#endif

bool ADRMiningGameModeBase::ShouldSpawnAtStartSpot(AController*)
{
	// 팀 변경을 반영하기 위해 최초 접속 시 저장된 StartSpot을 재사용하지 않는다.
	return false;
}

bool ADRMiningGameModeBase::StartGame()
{
	if (!HasAuthority() || GameFlowState != EDRGameFlowState::WaitingForPlayers)
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(GameResultTimerHandle);
	GetWorldTimerManager().ClearTimer(GameResultCountdownTimerHandle);
	RecalculateGameDuration();
	if (GameDuration <= 0.f || GamePhases.IsEmpty())
	{
		return false;
	}
	int32 Elapsed = 0;
	int32 PreviousPhase = INDEX_NONE;
	bool bHasCentralOpeningBoundary = false;
	for (const FDRGamePhaseConfig& Phase : GamePhases)
	{
		if (Phase.PhaseIndex <= PreviousPhase || Phase.DurationSeconds <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Game phases need increasing indices and positive durations."));
			return false;
		}
		bHasCentralOpeningBoundary |= Elapsed == 60;
		Elapsed += Phase.DurationSeconds;
		PreviousPhase = Phase.PhaseIndex;
	}
	if (!bHasCentralOpeningBoundary)
	{
		UE_LOG(LogTemp, Error, TEXT("Game phases must start a control phase at elapsed 60 seconds."));
		return false;
	}

	ResetGameState();
	CurrentPhaseArrayIndex = 0;
	PhaseRemainingSeconds = FMath::Max(1, GamePhases[CurrentPhaseArrayIndex].DurationSeconds);
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameEndDebugText(FString());
		MiningGameState->ResetMatchHUDState();
		MiningGameState->SetGameResultText(FText::GetEmpty());
		MiningGameState->SetControlZoneResult(FDRControlZoneGameResult());
		MiningGameState->SetGameTimerState(0);
	}
	SetGameFlowState(EDRGameFlowState::Loading);
	bPhaseCarversReady = !HasPhaseCarvers(GamePhases[0].PhaseIndex);
	UpdateReplicatedGamePhase();

	if (bPhaseCarversReady)
	{
		NotifyGameStartCarversReady();
	}
	return true;
}

void ADRMiningGameModeBase::BeginPlaying()
{
	if (!HasAuthority() || GameFlowState != EDRGameFlowState::Countdown)
	{
		return;
	}

	SetGameFlowState(EDRGameFlowState::Playing);
	for (TActorIterator<ADRGameStartActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->NotifyGameStarted();
	}
	StartTeamSwitchTimer();
	GameRemainingSeconds = FMath::CeilToInt(GameDuration);
	UpdateReplicatedGamePhase();
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameTimerState(GameRemainingSeconds);
	}
	GetWorldTimerManager().SetTimer(
		GameTimerHandle,
		this,
		&ThisClass::TickGameTimer,
		1.f,
		true);
}

void ADRMiningGameModeBase::NotifyGameStartCarversReady()
{
	if (!HasAuthority() || GameFlowState != EDRGameFlowState::Loading)
	{
		return;
	}
	// 메쉬 판정 비용은 준비 중 한 번 지불하고 플레이 중에는 마스크를 재사용한다.
	for (TActorIterator<ADRSnowControlZone> It(GetWorld()); It; ++It)
	{
		if (!It->PrepareForGame())
		{
			bPhaseCarveFailed = true;
			SetGameFlowState(EDRGameFlowState::WaitingForPlayers);
			return;
		}
	}

	ADRGameStartActor* Source = CountdownSource.Get();
	if (!IsValid(Source))
	{
		SetGameFlowState(EDRGameFlowState::Countdown);
		BeginPlaying();
		return;
	}

	SetGameFlowState(EDRGameFlowState::Countdown);
	Source->SetCountdownSecondsRemaining(CountdownRemainingSeconds);
	GetWorldTimerManager().SetTimer(
		GameStartTimerHandle, this, &ThisClass::TickGameStartCountdown, 1.f, true);
}

void ADRMiningGameModeBase::TickGameTimer()
{
	if (!IsGameStarted())
	{
		return;
	}

	--GameRemainingSeconds;
	--PhaseRemainingSeconds;
	if (GameRemainingSeconds <= 0)
	{
		EndGame();
		return;
	}
	if (PhaseRemainingSeconds <= 0)
	{
		AdvanceGamePhase();
	}

	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameTimerState(GameRemainingSeconds);
	}
	UpdateReplicatedGamePhase();
	UpdateControlZoneActivation();
}

void ADRMiningGameModeBase::AdvanceGamePhase()
{
	++CurrentPhaseArrayIndex;
	if (!GamePhases.IsValidIndex(CurrentPhaseArrayIndex))
	{
		CurrentPhaseArrayIndex = INDEX_NONE;
		PhaseRemainingSeconds = 0;
		return;
	}

	PhaseRemainingSeconds = FMath::Max(1, GamePhases[CurrentPhaseArrayIndex].DurationSeconds);
	bPhaseCarversReady = !HasPhaseCarvers(GamePhases[CurrentPhaseArrayIndex].PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[GameFlow] Phase=%d Remaining=%d GameRemaining=%d"),
		GamePhases[CurrentPhaseArrayIndex].PhaseIndex, PhaseRemainingSeconds, GameRemainingSeconds);
}

void ADRMiningGameModeBase::UpdateReplicatedGamePhase()
{
	ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>();
	if (!IsValid(MiningGameState) || !GamePhases.IsValidIndex(CurrentPhaseArrayIndex))
	{
		if (IsValid(MiningGameState))
		{
			MiningGameState->SetGamePhaseState(INDEX_NONE, 0, TArray<FText>());
			MiningGameState->SetPhaseCountdown(FDRPhaseCountdownState());
		}
		return;
	}

	const FDRGamePhaseConfig& Phase = GamePhases[CurrentPhaseArrayIndex];
	MiningGameState->SetGamePhaseState(
		Phase.PhaseIndex,
		PhaseRemainingSeconds,
		Phase.PlayerMessages);
	MiningGameState->SetPhaseCountdown(IsGameStarted()
		? Phase.MakeCountdownState(PhaseRemainingSeconds) : FDRPhaseCountdownState());
}

bool ADRMiningGameModeBase::HasPhaseCarvers(int32 PhaseIndex) const
{
	for (TActorIterator<ADRMeshVoxelCarver> It(GetWorld()); It; ++It)
	{
		if (It->ShouldCarveOnGameStart(PhaseIndex))
		{
			return true;
		}
	}
	return false;
}

void ADRMiningGameModeBase::NotifyPhaseCarversReady(int32 PhaseIndex, bool bSucceeded)
{
	if (!HasAuthority() || !GamePhases.IsValidIndex(CurrentPhaseArrayIndex))
	{
		return;
	}
	if (!bSucceeded)
	{
		bPhaseCarveFailed = true;
		UE_LOG(LogTemp, Error, TEXT("Phase %d carve failed; control zones stay locked."), PhaseIndex);
		if (GameFlowState == EDRGameFlowState::Loading)
		{
			SetGameFlowState(EDRGameFlowState::WaitingForPlayers);
		}
		return;
	}
	if (GamePhases[CurrentPhaseArrayIndex].PhaseIndex != PhaseIndex)
	{
		return;
	}
	bPhaseCarversReady = true;
	if (GameFlowState == EDRGameFlowState::Loading)
	{
		NotifyGameStartCarversReady();
	}
	else
	{
		UpdateControlZoneActivation();
	}
}

void ADRMiningGameModeBase::UpdateControlZoneActivation()
{
	if (!IsGameStarted() || !bPhaseCarversReady || bPhaseCarveFailed
		|| !GamePhases.IsValidIndex(CurrentPhaseArrayIndex)
		|| FMath::CeilToInt(GameDuration) - GameRemainingSeconds < 60)
	{
		return;
	}
	for (TActorIterator<ADRMeshVoxelCarver> It(GetWorld()); It; ++It)
	{
		if (It->IsCarving())
		{
			return;
		}
	}
	for (TActorIterator<ADRSnowControlZone> It(GetWorld()); It; ++It)
	{
		It->ActivateForPhase(GamePhases[CurrentPhaseArrayIndex].PhaseIndex);
	}
}

void ADRMiningGameModeBase::HandleControlZoneCompleted(ADRSnowControlZone* Zone)
{
	if (!HasAuthority() || !IsGameStarted() || !IsValid(Zone) || !IsValid(GameState)
		|| !Zone->TryClaimCompletionReward())
	{
		return;
	}
	const int32 WinningTeam = Zone->GetLeadingTeamId();
	float TotalReceived = 0.f;
	for (APlayerState* Player : GameState->PlayerArray)
	{
		ADRPlayerState* Recipient = Cast<ADRPlayerState>(Player);
		if (!IsValid(Recipient) || Recipient->IsOnlyASpectator() || Recipient->GetTeamId() != WinningTeam)
		{
			continue;
		}
		// 팀원별 실제 지급량을 합산하고, 이후 개인 소비와는 분리한다.
		const float PreviousGauge = Recipient->GetSnowGauge();
		Recipient->AddSnowGauge(FMath::Max(0.f, Zone->GetRewardSnowGauge()));
		TotalReceived += FMath::Max(0.f, Recipient->GetSnowGauge() - PreviousGauge);
		UAbilitySystemComponent* ASC = Recipient->GetAbilitySystemComponent();
		if (!IsValid(ASC))
		{
			continue;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(Zone);
		for (const TSubclassOf<UGameplayEffect>& Effect : Zone->GetRewardEffects())
		{
			if (Effect)
			{
				const int32 PreviousStacks = ASC->GetGameplayEffectCount(Effect, nullptr, false);
				const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(
					Effect->GetDefaultObject<UGameplayEffect>(), 1.f, Context);
				if (Handle.IsValid() && ASC->GetGameplayEffectCount(Effect, nullptr, false) > PreviousStacks)
				{
					ControlZoneRewardEffects.Add(Handle);
				}
			}
		}
	}
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->AddControlZoneReward(WinningTeam, TotalReceived);
	}
}

void ADRMiningGameModeBase::ClearControlZoneRewardEffects()
{
	for (const FActiveGameplayEffectHandle& Handle : ControlZoneRewardEffects)
	{
		if (UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent())
		{
			// 거점 보상이 추가한 스택만 회수한다.
			ASC->RemoveActiveGameplayEffect(Handle, 1);
		}
	}
	ControlZoneRewardEffects.Reset();
}

void ADRMiningGameModeBase::RecalculateGameDuration()
{
	GameDuration = 0.f;
	for (const FDRGamePhaseConfig& Phase : GamePhases)
	{
		GameDuration += FMath::Max(1, Phase.DurationSeconds);
	}
}

void ADRMiningGameModeBase::EndGame()
{
	if (!HasAuthority() || !IsGameStarted())
	{
		return;
	}

	SetGameFlowState(EDRGameFlowState::Results);
	for (TActorIterator<ADRMeshVoxelCarver> It(GetWorld()); It; ++It)
	{
		It->RestartCarveBatch();
	}
	GameRemainingSeconds = 0;
	CurrentPhaseArrayIndex = INDEX_NONE;
	PhaseRemainingSeconds = 0;
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->SetGameTimerState(0);
	}
	UpdateReplicatedGamePhase();

	double WeightedTeamScores[2] = {0.0, 0.0};
	TArray<FString> ZoneDebugTexts;
	for (TActorIterator<ADRSnowControlZone> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->FreezeForGameEnd();
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
		FDRControlZoneGameResult Result;
		Result.bHasResult = true;
		Result.Team0Ratio = WeightedScoreTotal > 0.0 ? WeightedTeamScores[0] / WeightedScoreTotal : 0.f;
		Result.Team1Ratio = WeightedScoreTotal > 0.0 ? WeightedTeamScores[1] / WeightedScoreTotal : 0.f;
		Result.WinningTeamId = FMath::IsNearlyEqual(WeightedTeamScores[0], WeightedTeamScores[1], 0.0001)
			? INDEX_NONE : WeightedTeamScores[0] > WeightedTeamScores[1] ? 0 : 1;
		MiningGameState->SetControlZoneResult(Result);
		MiningGameState->RequestControlZoneCleanup();

	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Iterator->Get());
		UDRInventoryComponent* Inventory = IsValid(PlayerController) ? PlayerController->GetInventoryComponent() : nullptr;
		if (IsValid(Inventory))
		{
			Inventory->ResetWeaponUpgrades();
		}
	}

	TArray<ADRSnowControlZone*> Zones;
	for (TActorIterator<ADRSnowControlZone> It(GetWorld()); It; ++It)
	{
		Zones.Add(*It);
	}
	PendingZoneCleanups = Zones.Num() + 1;
	for (ADRSnowControlZone* Zone : Zones)
	{
		const TWeakObjectPtr<ADRMiningGameModeBase> WeakThis(this);
		if (!Zone->StartEndCleanup([WeakThis](bool bSucceeded)
		{
			if (!bSucceeded)
			{
				UE_LOG(LogTemp, Error, TEXT("Control zone cleanup did not finish."));
			}
			if (ADRMiningGameModeBase* Mode = WeakThis.Get())
			{
				Mode->FinishControlZoneCleanup();
			}
		}))
		{
			UE_LOG(LogTemp, Error, TEXT("Control zone cleanup failed: %s."), *Zone->GetName());
			FinishControlZoneCleanup();
		}
	}
	FinishControlZoneCleanup();
}

void ADRMiningGameModeBase::FinishControlZoneCleanup()
{
	if (!IsGameEnded() || PendingZoneCleanups <= 0 || --PendingZoneCleanups > 0)
	{
		return;
	}
	// 조형물 정리가 끝난 뒤 결과 표시 시간을 보장한다.
	GetWorldTimerManager().SetTimer(
		GameResultTimerHandle,
		this,
		&ThisClass::ReturnToWaiting,
		FMath::Max(0.1f, GameResultDisplayDuration),
		false);
	TickGameResultCountdown();
	GetWorldTimerManager().SetTimer(GameResultCountdownTimerHandle,
		this, &ThisClass::TickGameResultCountdown, 1.f, true);
}

void ADRMiningGameModeBase::TickGameResultCountdown()
{
	if (!HasAuthority() || !IsGameEnded())
	{
		return;
	}
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		const float Remaining = GetWorldTimerManager().GetTimerRemaining(GameResultTimerHandle);
		MiningGameState->SetResultCountdown(
			FMath::Max(0, FMath::CeilToInt(Remaining)), GameResultCountdownText);
	}
}

// 결과 표시가 끝난 뒤에만 다음 경기의 준비를 허용한다.
void ADRMiningGameModeBase::ReturnToWaiting()
{
	if (!HasAuthority() || !IsGameEnded())
	{
		return;
	}

	SetGameFlowState(EDRGameFlowState::WaitingForPlayers);
	ClearControlZoneRewardEffects();
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->ResetMatchHUDState();
		MiningGameState->SetGameResultText(FText::GetEmpty());
		MiningGameState->SetControlZoneResult(FDRControlZoneGameResult());
		MiningGameState->SetGameTimerState(0);
	}
	for (TActorIterator<ADRGameStartActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->ResetForNextGame();
	}
}

// 이전 상태가 소유한 타이머를 정리한 후 클라이언트에 새 상태를 전달한다.
void ADRMiningGameModeBase::SetGameFlowState(EDRGameFlowState NewState)
{
	if (!HasAuthority() || GameFlowState == NewState)
	{
		return;
	}

	if (GameFlowState == EDRGameFlowState::Countdown)
	{
		GetWorldTimerManager().ClearTimer(GameStartTimerHandle);
		if (CountdownSource.IsValid())
		{
			CountdownSource->SetCountdownSecondsRemaining(0);
		}
		CountdownSource.Reset();
		CountdownRemainingSeconds = 0;
	}
	else if (GameFlowState == EDRGameFlowState::Playing)
	{
		GetWorldTimerManager().ClearTimer(TeamSwitchTimerHandle);
		GetWorldTimerManager().ClearTimer(GameTimerHandle);
	}
	else if (GameFlowState == EDRGameFlowState::Results)
	{
		GetWorldTimerManager().ClearTimer(GameResultTimerHandle);
		GetWorldTimerManager().ClearTimer(GameResultCountdownTimerHandle);
	}

	const bool bWasPreparing = GameFlowState == EDRGameFlowState::Loading
		|| GameFlowState == EDRGameFlowState::Countdown || GameFlowState == EDRGameFlowState::Results;
	const bool bIsPreparing = NewState == EDRGameFlowState::Loading
		|| NewState == EDRGameFlowState::Countdown || NewState == EDRGameFlowState::Results;
	UE_LOG(LogTemp, Log, TEXT("[GameFlow] %s -> %s PhaseArray=%d PhaseRemaining=%d GameRemaining=%d"),
		*StaticEnum<EDRGameFlowState>()->GetNameStringByValue(static_cast<int64>(GameFlowState)),
		*StaticEnum<EDRGameFlowState>()->GetNameStringByValue(static_cast<int64>(NewState)),
		CurrentPhaseArrayIndex, PhaseRemainingSeconds, GameRemainingSeconds);
	GameFlowState = NewState;
	if (bWasPreparing != bIsPreparing)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			SetGamePreparingBlocked(Cast<ADRPlayerState>(PlayerState), bIsPreparing);
		}
	}
	if (ADRMiningGameStateBase* MiningGameState = GetGameState<ADRMiningGameStateBase>())
	{
		FText FlowMessage;
		if (NewState == EDRGameFlowState::Loading)
		{
			FlowMessage = GameLoadingMessage;
		}
		else if (NewState == EDRGameFlowState::Countdown)
		{
			FlowMessage = GameStartCountdownText;
		}
		else if (NewState == EDRGameFlowState::WaitingForPlayers && bPhaseCarveFailed)
		{
			FlowMessage = GamePreparationFailedMessage;
		}
		MiningGameState->SetGameFlowState(NewState, FlowMessage);
	}
}

void ADRMiningGameModeBase::RequestGameStart(ADRGameStartActor* Source, int32 CountdownSeconds)
{
	if (!HasAuthority() || !IsValid(Source)
		|| GameFlowState != EDRGameFlowState::WaitingForPlayers)
	{
		return;
	}
	if (Source->GetTotalPlayerCount() <= 0
		|| Source->GetReadyPlayerCount() < Source->GetTotalPlayerCount())
	{
		return;
	}

	CountdownSource = Source;
	CountdownRemainingSeconds = FMath::Max(1, CountdownSeconds);
	if (!StartGame())
	{
		CountdownSource.Reset();
		CountdownRemainingSeconds = 0;
	}
}

void ADRMiningGameModeBase::CancelGameCountdown(ADRGameStartActor* Source)
{
	if (HasAuthority() && GameFlowState == EDRGameFlowState::Countdown
		&& CountdownSource.Get() == Source)
	{
		SetGameFlowState(EDRGameFlowState::WaitingForPlayers);
	}
}

void ADRMiningGameModeBase::TickGameStartCountdown()
{
	if (GameFlowState != EDRGameFlowState::Countdown)
	{
		return;
	}

	ADRGameStartActor* Source = CountdownSource.Get();
	if (!IsValid(Source))
	{
		SetGameFlowState(EDRGameFlowState::WaitingForPlayers);
		return;
	}

	--CountdownRemainingSeconds;
	Source->SetCountdownSecondsRemaining(CountdownRemainingSeconds);
	if (CountdownRemainingSeconds <= 0)
	{
		BeginPlaying();
	}
}

void ADRMiningGameModeBase::SetGamePreparingBlocked(
	ADRPlayerState* PlayerState,
	bool bBlocked) const
{
	UAbilitySystemComponent* AbilitySystem = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	FGameplayTagContainer AllAbilityTags;
	AllAbilityTags.AddTag(DRGameplayTags::Ability_Root);
	const bool bHasPreparingTag = AbilitySystem->HasMatchingGameplayTag(
		DRGameplayTags::State_GamePreparing);
	if (bBlocked && !bHasPreparingTag)
	{
		// 준비가 시작되면 진행 중 행동까지 종료하고 새 행동을 막는다.
		AbilitySystem->AddLooseGameplayTag(
			DRGameplayTags::State_GamePreparing,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		AbilitySystem->BlockAbilitiesWithTags(AllAbilityTags);
		AbilitySystem->CancelAbilities(&AllAbilityTags);
	}
	else if (!bBlocked && bHasPreparingTag)
	{
		AbilitySystem->RemoveLooseGameplayTag(
			DRGameplayTags::State_GamePreparing,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		AbilitySystem->UnBlockAbilitiesWithTags(AllAbilityTags);
	}
}

void ADRMiningGameModeBase::ResetGameState()
{
	ClearControlZoneRewardEffects();
	PendingZoneCleanups = 0;
	bPhaseCarveFailed = false;
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
	GetWorldTimerManager().ClearTimer(GameStartTimerHandle);
	GetWorldTimerManager().ClearTimer(TeamSwitchTimerHandle);
	GetWorldTimerManager().ClearTimer(GameTimerHandle);
	GetWorldTimerManager().ClearTimer(GameResultTimerHandle);
	GetWorldTimerManager().ClearTimer(GameResultCountdownTimerHandle);

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

	if (PlayerController->IsLocalController())
	{
		// 호스트는 이미 권위 월드에 있으므로 직렬화/전송/재적용이 필요 없다.
		HandleSnowJoinSnapshotApplied(PlayerController, false);
	}
	else if (!TryStartSnowJoinSnapshot(PlayerController))
	{
		// 복원할 상태를 만들지 못한 접속자를 불완전한 월드로 입장시키지 않는다.
		PlayerController->GetSnowJoinComponent()->FailSnowJoin(TEXT("CheckpointCreationFailed"));
		return;
	}

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
	PendingSnowJoinPlayers.Add(PlayerController);
	OnJoinSnapshotStarted.Broadcast();
	if (!SnowSubsystem->CreateCheckpoint(MiningGameState->GetSnowOperationSequence()))
	{
		FinishPendingSnowJoin(PlayerController, EDRSnowJoinSnapshotResult::InvalidCheckpoint);
		return false;
	}

	FDRSnowJoinCheckpoint Checkpoint;
	if (!SnowSubsystem->GetLatestCheckpoint(Checkpoint))
	{
		FinishPendingSnowJoin(PlayerController, EDRSnowJoinSnapshotResult::InvalidCheckpoint);
		return false;
	}

	PlayerController->GetSnowJoinComponent()->BeginSnowJoinSnapshot(MoveTemp(Checkpoint));
	return true;
}

void ADRMiningGameModeBase::FinishPendingSnowJoin(
	APlayerController* PlayerController, EDRSnowJoinSnapshotResult Result)
{
	// 실패 통지와 실제 Logout이 연달아 와도 시작 이벤트 하나당 한 번만 해제한다.
	if (PendingSnowJoinPlayers.Remove(PlayerController) > 0)
	{
		OnJoinSnapshotFinished.Broadcast(Result);
	}
}

void ADRMiningGameModeBase::HandleSnowJoinSnapshotFailed(APlayerController* PlayerController)
{
	FinishPendingSnowJoin(PlayerController, EDRSnowJoinSnapshotResult::Disconnected);
}

bool ADRMiningGameModeBase::HandleSnowJoinSnapshotApplied(
	APlayerController* PlayerController,
	bool bNotifySnapshotFinished)
{
	bool bPlayerRestarted = IsValid(PlayerController) && IsValid(PlayerController->GetPawn());
	if (IsValid(PlayerController) && !IsValid(PlayerController->GetPawn()))
	{
		PlayerController->ChangeState(NAME_Playing);
		PlayerController->ClientGotoState(NAME_Playing);
		RestartPlayer(PlayerController);
		SetGamePreparingBlocked(PlayerController->GetPlayerState<ADRPlayerState>(),
			GameFlowState == EDRGameFlowState::Loading || GameFlowState == EDRGameFlowState::Countdown
				|| GameFlowState == EDRGameFlowState::Results);

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
		FinishPendingSnowJoin(PlayerController, bPlayerRestarted
			? EDRSnowJoinSnapshotResult::Applied : EDRSnowJoinSnapshotResult::Disconnected);
	}

	return bPlayerRestarted;
}

void ADRMiningGameModeBase::Logout(AController* Exiting)
{
	HandleSnowJoinSnapshotFailed(Cast<APlayerController>(Exiting));
	const ADRPlayerState* PlayerState =
		IsValid(Exiting) ? Exiting->GetPlayerState<ADRPlayerState>() : nullptr;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Logout] Player=%s PlayerId=%d TeamId=%d"),
		*GetNameSafe(PlayerState),
		IsValid(PlayerState) ? PlayerState->GetPlayerId() : INDEX_NONE,
		IsValid(PlayerState) ? PlayerState->GetTeamId() : INDEX_NONE);

	// Controller::Destroyed는 Logout 이후 PlayerState를 제거한다.
	Super::Logout(Exiting);
	GetWorldTimerManager().SetTimerForNextTick(
		this, &ThisClass::RefreshGameStartPlayerRoster);
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

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDRPhaseCountdownTest, "DeepRaiders.GameFlow.PhaseCountdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRPhaseCountdownTest::RunTest(const FString&)
{
	FDRGamePhaseConfig Phase;
	Phase.DurationSeconds = 5;
	Phase.EntryCountdownSeconds = 4;
	Phase.ExitCountdownSeconds = 3;
	Phase.EntryCountdownText = FText::FromString(TEXT("Entry {Seconds}"));
	Phase.ExitCountdownText = FText::FromString(TEXT("Exit {Seconds}"));
	TestEqual(TEXT("Both disabled"), Phase.MakeCountdownState(5).RemainingSeconds, 0);
	Phase.bShowEntryCountdown = true;
	TestEqual(TEXT("Entry starts at four"), Phase.MakeCountdownState(5).RemainingSeconds, 4);
	TestEqual(TEXT("Entry expires"), Phase.MakeCountdownState(1).RemainingSeconds, 0);
	Phase.bShowExitCountdown = true;
	const FDRPhaseCountdownState Overlap = Phase.MakeCountdownState(3);
	TestTrue(TEXT("Exit wins overlap"), Overlap.bIsExitCountdown);
	TestEqual(TEXT("Exit uses phase remainder"), Overlap.RemainingSeconds, 3);
	TestTrue(TEXT("Exit keeps its own text"), Overlap.Text.EqualTo(Phase.ExitCountdownText));
	Phase.bShowEntryCountdown = false;
	TestEqual(TEXT("Exit not shown early"), Phase.MakeCountdownState(5).RemainingSeconds, 0);
	TestEqual(TEXT("No countdown after phase"), Phase.MakeCountdownState(0).RemainingSeconds, 0);
	return true;
}
#endif
