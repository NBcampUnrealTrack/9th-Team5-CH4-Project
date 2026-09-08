#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/GameStates/DRGameFlowState.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "DRMiningGameModeBase.generated.h"

struct FPropertyChangedChainEvent;

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
};

// 채굴 테스트/플레이용 GameState를 사용하는 GameMode이다.
UCLASS()
class DEEPRAIDERS_API ADRMiningGameModeBase : public AGameModeBase
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Flow")
	FText GameLoadingMessage = NSLOCTEXT(
		"DRGameFlow", "Loading", "지형을 준비하고 있습니다.");

private:
	void ResetGameState();
	void TickGameTimer();
	void AdvanceGamePhase();
	void UpdateReplicatedGamePhase();
	void RecalculateGameDuration();
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

	FTimerHandle GameStartTimerHandle;
	TWeakObjectPtr<class ADRGameStartActor> CountdownSource;
	int32 CountdownRemainingSeconds = 0;

	FTimerHandle TeamSwitchTimerHandle;
	FTimerHandle GameTimerHandle;
	FTimerHandle GameResultTimerHandle;
	int32 GameRemainingSeconds = 0;
	int32 CurrentPhaseArrayIndex = INDEX_NONE;
	int32 PhaseRemainingSeconds = 0;
	int32 ActiveTeamId = INDEX_NONE;

	void StartTeamSwitchTimer();
	void RefreshActiveTeam();
	void ApplyActiveTeam(bool bImmediate);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game|Flow",
		meta = (AllowPrivateAccess = "true"))
	EDRGameFlowState GameFlowState = EDRGameFlowState::WaitingForPlayers;

	void EnsureDevelopmentPlayerName(ADRPlayerState* PlayerState) const;
};
