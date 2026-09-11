#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/GameStates/DRGameFlowState.h"
#include "RoomServiceGameModeBase.h"
#include "TimerManager.h"
#include "ActiveGameplayEffectHandle.h"
#include "DRMiningGameModeBase.generated.h"

struct FPropertyChangedChainEvent;
struct FDRPhaseCountdownState;

enum class EDRSnowJoinSnapshotResult : uint8
{
	Applied,
	InvalidCheckpoint,
	Disconnected,
};

DECLARE_MULTICAST_DELEGATE(FOnJoinSnapshotStarted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJoinSnapshotFinished, EDRSnowJoinSnapshotResult);

USTRUCT(BlueprintType)
struct FDRGamePhaseConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase")
	int32 PhaseIndex = 0;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Game Phase",
		meta = (ClampMin = "1", Units = "s"))
	int32 DurationSeconds = 180;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase")
	TArray<FText> PlayerMessages;

	// 진입은 페이즈 시작 직후, 종료는 끝나기 직전의 표시 구간이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase|Countdown")
	bool bShowEntryCountdown = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase|Countdown",
		meta = (ClampMin = "1", EditCondition = "bShowEntryCountdown", Units = "s"))
	int32 EntryCountdownSeconds = 3;

	// {Seconds} 자리에 남은 초를 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase|Countdown",
		meta = (EditCondition = "bShowEntryCountdown"))
	FText EntryCountdownText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase|Countdown")
	bool bShowExitCountdown = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase|Countdown",
		meta = (ClampMin = "1", EditCondition = "bShowExitCountdown", Units = "s"))
	int32 ExitCountdownSeconds = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Phase|Countdown",
		meta = (EditCondition = "bShowExitCountdown"))
	FText ExitCountdownText;

	FDRPhaseCountdownState MakeCountdownState(int32 RemainingSeconds) const;
};

// 채굴 테스트/플레이용 GameState를 사용하는 GameMode이다.
UCLASS()
class DEEPRAIDERS_API ADRMiningGameModeBase : public ARoomServiceGameModeBase
{
	GENERATED_BODY()

public:
	ADRMiningGameModeBase();

	/** 모든 경기 데이터를 초기화한 뒤 경기를 시작한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	bool StartGame();

	UFUNCTION(BlueprintPure, Category = "Game")
	bool IsGameStarted() const { return GameFlowState == EDRGameFlowState::Playing; }

	UFUNCTION(BlueprintPure, Category = "Game")
	bool IsGameEnded() const { return GameFlowState == EDRGameFlowState::Results; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	void EndGame();

	UFUNCTION(BlueprintPure, Category = "Game|Flow")
	EDRGameFlowState GetGameFlowState() const { return GameFlowState; }

	// 준비 액터는 인원 판정만 하고 실제 게임 시작 흐름은 GameMode에 요청한다.
	void RequestGameStart(class ADRGameStartActor* Source, int32 CountdownSeconds);
	void CancelGameCountdown(class ADRGameStartActor* Source);
	void NotifyGameStartCarversReady();
	void NotifyPhaseCarversReady(int32 PhaseIndex, bool bSucceeded);
	void HandleControlZoneCompleted(class ADRSnowControlZone* Zone);

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	FOnJoinSnapshotStarted OnJoinSnapshotStarted;
	FOnJoinSnapshotFinished OnJoinSnapshotFinished;

	// 중도 접속자의 눈 스냅샷 적용이 끝난 뒤 실제 플레이어를 생성한다.
	bool HandleSnowJoinSnapshotApplied(
		APlayerController* PlayerController,
		bool bNotifySnapshotFinished = true);
	void HandleSnowJoinSnapshotFailed(APlayerController* PlayerController);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(
		FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Team Movement",
		meta = (ClampMin = "0.01", Units = "s"))
	float TeamSwitchInterval = 10.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Game|Phase")
	TArray<FDRGamePhaseConfig> GamePhases;

	UPROPERTY(
		VisibleDefaultsOnly,
		BlueprintReadOnly,
		Category = "Game|Phase",
		meta = (Units = "s"))
	float GameDuration = 180.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Game",
		meta = (ClampMin = "0.1", Units = "s"))
	float GameResultDisplayDuration = 5.f;

	// {Seconds} 자리에 남은 초를 넣는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Flow")
	FText GameStartCountdownText = NSLOCTEXT(
		"DRGameFlow", "StartCountdown", "게임 시작까지 {Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Flow")
	FText GameResultCountdownText = NSLOCTEXT(
		"DRGameFlow", "ResultCountdown", "다음 준비까지 {Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Flow")
	FText GameLoadingMessage = NSLOCTEXT(
		"DRGameFlow", "Loading", "지형을 준비하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Flow")
	FText GamePreparationFailedMessage = NSLOCTEXT(
		"DRGameFlow", "PreparationFailed", "지형을 준비하지 못했습니다.");

public:
	bool IsSnowJoinInProgress() const { return !PendingSnowJoinPlayers.IsEmpty(); }

private:
	void ResetGameState();
	void TickGameTimer();
	void AdvanceGamePhase();
	void UpdateReplicatedGamePhase();
	void RecalculateGameDuration();
	void UpdateControlZoneActivation();
	void FinishControlZoneCleanup();
	void TickGameResultCountdown();
	bool HasPhaseCarvers(int32 PhaseIndex) const;
	void ClearControlZoneRewardEffects();
	void ReturnToWaiting();
	void SetGameFlowState(EDRGameFlowState NewState);
	void SetGamePreparingBlocked(class ADRPlayerState* PlayerState, bool bBlocked) const;
	void TickGameStartCountdown();
	void BeginPlaying();
	void RefreshGameStartPlayerRoster();
	int32 AssignBalancedTeam(class ADRPlayerState* PlayerState) const;
	bool TryStartSnowJoinSnapshot(class ADRPlayerController* PlayerController);
	void FinishPendingSnowJoin(APlayerController* PlayerController, EDRSnowJoinSnapshotResult Result);
	TSet<TWeakObjectPtr<APlayerController>> PendingSnowJoinPlayers;
	friend class FDRSnowJoinLifecycleTest;
	friend class FDRVoxelDepositTest;

	FTimerHandle GameStartTimerHandle;
	TWeakObjectPtr<class ADRGameStartActor> CountdownSource;
	int32 CountdownRemainingSeconds = 0;

	FTimerHandle TeamSwitchTimerHandle;
	FTimerHandle GameTimerHandle;
	FTimerHandle GameResultTimerHandle;
	FTimerHandle GameResultCountdownTimerHandle;
	int32 GameRemainingSeconds = 0;
	int32 CurrentPhaseArrayIndex = INDEX_NONE;
	int32 PhaseRemainingSeconds = 0;
	int32 ActiveTeamId = INDEX_NONE;
	bool bPhaseCarversReady = false;
	bool bPhaseCarveFailed = false;
	int32 PendingZoneCleanups = 0;
	TArray<FActiveGameplayEffectHandle> ControlZoneRewardEffects;

	void StartTeamSwitchTimer();
	void RefreshActiveTeam();
	void ApplyActiveTeam(bool bImmediate);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game|Flow",
		meta = (AllowPrivateAccess = "true"))
	EDRGameFlowState GameFlowState = EDRGameFlowState::WaitingForPlayers;

	void EnsureDevelopmentPlayerName(ADRPlayerState* PlayerState) const;
};
