#pragma once

#include "CoreMinimal.h"
#include "DRGameFlowState.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "GameFramework/GameStateBase.h"
#include "DRMiningGameStateBase.generated.h"

class ADRTeleportPoint;
class AVoxelWorld;
class FLifetimeProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDRGameTimerChanged,
	int32,
	RemainingSeconds,
	bool,
	bGameStarted,
	bool,
	bGameEnded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRGameEndDebugTextChanged,
	const FString&,
	DebugText);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRGameResultTextChanged,
	const FText&,
	ResultText);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDRGamePhaseChanged,
	int32,
	PhaseIndex,
	int32,
	PhaseRemainingSeconds,
	const TArray<FText>&,
	PlayerMessages);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRGameFlowStateChanged, EDRGameFlowState, GameFlowState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRGameFlowMessageChanged, const FText&, GameFlowMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRMatchHUDStateChanged);

USTRUCT()
struct FDRTeamRegisteredTeleportPoint
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TeamId = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<ADRTeleportPoint> TeleportPoint;
};

struct FDRSnowOperationBatcher;
struct FDRSnowLoadTest;

USTRUCT(BlueprintType)
struct FDRPhaseCountdownState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 RemainingSeconds = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bIsExitCountdown = false;

	UPROPERTY(BlueprintReadOnly)
	FText Text;
};

USTRUCT(BlueprintType)
struct FDRControlZoneGameResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bHasResult = false;

	UPROPERTY(BlueprintReadOnly)
	float Team0Ratio = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float Team1Ratio = 0.f;

	// INDEX_NONE은 동률 또는 양 팀 모두 점유량이 없는 경우다.
	UPROPERTY(BlueprintReadOnly)
	int32 WinningTeamId = INDEX_NONE;
};

UCLASS()
class DEEPRAIDERS_API ADRMiningGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetGameTimerState(int32 RemainingSeconds);
	void SetGameFlowState(
		EDRGameFlowState NewState,
		const FText& NewMessage = FText::GetEmpty());

	UFUNCTION(BlueprintPure, Category = "Game|Flow")
	EDRGameFlowState GetGameFlowState() const { return GameFlowState; }

	UFUNCTION(BlueprintPure, Category = "Game|Flow")
	FText GetGameFlowMessage() const { return GameFlowMessage; }

	UPROPERTY(BlueprintAssignable, Category = "Game|Flow")
	FDRGameFlowStateChanged OnGameFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game|Flow")
	FDRGameFlowMessageChanged OnGameFlowMessageChanged;
	void SetGamePhaseState(
		int32 PhaseIndex,
		int32 RemainingSeconds,
		const TArray<FText>& PlayerMessages);
	void SetGameEndDebugText(const FString& DebugText);
	void SetGameResultText(const FText& ResultText);
	void SetPhaseCountdown(const FDRPhaseCountdownState& Countdown);
	void SetControlZoneResult(const FDRControlZoneGameResult& Result);
	void RequestControlZoneCleanup();
	void AddControlZoneReward(int32 TeamId, float Amount);
	void SetResultCountdown(int32 RemainingSeconds, const FText& CountdownText);
	void ResetMatchHUDState();

	UFUNCTION(BlueprintPure, Category = "Game|Reward")
	float GetControlZoneRewardTotal(int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "Game|Result")
	int32 GetResultRemainingSeconds() const { return ResultRemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Game|Result")
	FText GetResultCountdownText() const { return ResultCountdownText; }

	UPROPERTY(BlueprintAssignable, Category = "Game|HUD")
	FDRMatchHUDStateChanged OnMatchHUDStateChanged;

	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	FDRPhaseCountdownState GetPhaseCountdown() const { return PhaseCountdown; }

	UFUNCTION(BlueprintPure, Category = "Game|Result")
	FDRControlZoneGameResult GetControlZoneResult() const { return ControlZoneResult; }

	int32 GetGameRemainingSeconds() const { return GameRemainingSeconds; }
	bool IsGameStarted() const { return GameFlowState == EDRGameFlowState::Playing; }
	bool IsGameEnded() const { return GameFlowState == EDRGameFlowState::Results; }
	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	int32 GetPhaseRemainingSeconds() const { return PhaseRemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	TArray<FText> GetCurrentPhaseMessages() const { return CurrentPhaseMessages; }
	UFUNCTION(BlueprintPure, Category = "Game")
	FString GetGameEndDebugText() const { return GameEndDebugText; }

	UFUNCTION(BlueprintPure, Category = "Game")
	FText GetGameResultText() const { return GameResultText; }

	UPROPERTY(BlueprintAssignable, Category = "Game")
	FDRGameTimerChanged OnGameTimerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game")
	FDRGameEndDebugTextChanged OnGameEndDebugTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game")
	FDRGameResultTextChanged OnGameResultTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game|Phase")
	FDRGamePhaseChanged OnGamePhaseChanged;

private:
	UFUNCTION()
	void OnRep_MatchHUDState();

	// 개인의 현재 잔액과 분리한, 이번 경기 거점 보상 누계다.
	UPROPERTY(ReplicatedUsing = OnRep_MatchHUDState)
	float Team0ControlZoneReward = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_MatchHUDState)
	float Team1ControlZoneReward = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_MatchHUDState)
	int32 ResultRemainingSeconds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchHUDState)
	FText ResultCountdownText;

	UFUNCTION()
	void OnRep_ControlZoneResult();

	UPROPERTY(ReplicatedUsing = OnRep_GamePhaseState)
	FDRPhaseCountdownState PhaseCountdown;

	UPROPERTY(ReplicatedUsing = OnRep_ControlZoneResult)
	FDRControlZoneGameResult ControlZoneResult;

	UFUNCTION()
	void OnRep_ZoneCleanupSequence();
	void TryClientZoneCleanup();

	UPROPERTY(ReplicatedUsing = OnRep_ZoneCleanupSequence)
	int32 ZoneCleanupSequence = INDEX_NONE;

	FTimerHandle ZoneCleanupRetryTimer;
	bool bClientZoneCleanupStarted = false;

	UFUNCTION()
	void OnRep_GameTimerState();

	UFUNCTION()
	void OnRep_GameEndDebugText();

	UFUNCTION()
	void OnRep_GameResultText();

	UFUNCTION()
	void OnRep_GamePhaseState();

	UPROPERTY(ReplicatedUsing = OnRep_GameTimerState)
	int32 GameRemainingSeconds = 0;

	UFUNCTION()
	void OnRep_GameFlowState();

	UFUNCTION()
	void OnRep_GameFlowMessage();

	UPROPERTY(ReplicatedUsing = OnRep_GameFlowState)
	EDRGameFlowState GameFlowState = EDRGameFlowState::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_GameFlowMessage)
	FText GameFlowMessage;

	UPROPERTY(ReplicatedUsing = OnRep_GameEndDebugText)
	FString GameEndDebugText;

	UPROPERTY(ReplicatedUsing = OnRep_GameResultText)
	FText GameResultText;

	UPROPERTY(ReplicatedUsing = OnRep_GamePhaseState)
	int32 CurrentPhaseIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_GamePhaseState)
	int32 PhaseRemainingSeconds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_GamePhaseState)
	TArray<FText> CurrentPhaseMessages;

#pragma region TerrainDig
public:
	void RegisterTerrainDig(const FDRTerrainDigOperation& Operation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyTerrainDig(const FDRTerrainDigOperation& Operation);

private:
	bool ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation);
#pragma endregion 

#pragma region Snow
public:
	void RegisterSnowAdd(const FDRSnowAddOperation& Operation);
	void RegisterSnowAdd(const FDRSnowAddOperation& Operation, float ServerAppliedAmount);
	void RegisterSnowRemove(
		const FDRSnowRemoveOperation& Operation);
	void RegisterSnowDeposit(const FDRVoxelDepositResult& Result);
	friend class FDRVoxelDepositTest;
	int32 GetSnowOperationSequence() const { return NextSnowOperationSequence; }
	void ResetSnowOperationState();
	void ResetSnowApplicationStateForCheckpoint(int32 CheckpointSequence);
	bool ApplySnowOperationRecord(const FDRSnowOperationRecord& Record);
	// Native, non-RPC diagnostic entry. Implementation is disabled in Shipping.
	void StartSnowLoadTest(const TArray<FString>& Args);
	void StopSnowLoadTest();

	// 새 경기를 시작할 때 서버와 모든 클라이언트의 복셀/지형 데이터를 초기화합니다.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ResetVoxelState();

	// 짧은 시간 동안 발생한 눈 변경 작업들을 배열로 묶어 한 번에 보내는 Reliable Multicast RPC
	// 패킷 내 배열 순서는 서버의 발생 순서(Sequence)와 동일합니다.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplySnowOperations(const TArray<FDRSnowOperationRecord>& Records);

private:
	// 서버 작업을 즉시 전송하거나 쿨타임 배치에 추가한다.
	void QueueSnowOperationForBroadcast(FDRSnowOperationRecord&& Record);
	void ClearSnowOperationBroadcasts();

	bool ApplySnowAddOnce(const FDRSnowOperationRecord& Record);
	bool ApplySnowRemoveOnce(const FDRSnowOperationRecord& Record);
	void HandleDirectionalSnowAddCompleted(
		int32 OperationSequence,
		int32 ApplicationGeneration,
		float AppliedAmount);
	bool IsSnowOperationReady(const FDRSnowOperationRecord& Record) const;
	bool IsSnowOperationApplied(int32 Sequence) const;
	bool HasPendingSnowOperation(int32 Sequence) const;
	void QueuePendingSnowOperation(const FDRSnowOperationRecord& Record);
	void TryApplyPendingSnowOperations();
	void StartPendingSnowRetry();
	void StopPendingSnowRetry();
	void ScheduleSnowReplayContinuation();
	AVoxelWorld* ResolveVoxelWorldByName(FName VoxelWorldName) const;

	// 서버 전송 큐와 rate limit의 수명을 GameState에 묶는다.
	TSharedPtr<FDRSnowOperationBatcher> SnowOperationBatcher;

	// 공통: 눈 작업 고유 번호 (서버: 순차 발급, 클라이언트: 중복 처리 방지용)
	int32 NextSnowOperationSequence = 0;

	// 클라이언트: 체크포인트 스냅샷으로 이미 처리 완료된 작업 번호 기준선
	int32 AppliedSnowCheckpointSequence = 0;

	// 클라이언트: 이미 로컬에 반영 완료된 작업 번호 목록 (중복 실행 방지)
	TSet<int32> AppliedSnowOperationSequences;

	// 클라이언트: 복셀 월드가 아직 로드되지 않아 생성을 기다리는 작업 목록
	TArray<FDRSnowOperationRecord> PendingSnowOperations;
	FTimerHandle PendingSnowRetryTimer;
	int32 ActiveDirectionalSnowOperationSequence = INDEX_NONE;
	int32 SnowApplicationGeneration = 0;
	bool bSnowReplayContinuationScheduled = false;
	bool bSnowReplayPumping = false;
	uint64 SnowReplayBudgetFrame = MAX_uint64;
	int32 SnowReplayStartsThisFrame = 0;
	double SnowReplayDispatchMsThisFrame = 0.0;
	TSharedPtr<FDRSnowLoadTest> SnowLoadTest;
#pragma endregion
	
#pragma region Teleport
public:
	void AddTeamRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint);
	void RemoveRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint);

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	bool CanTeamUseRegisteredTeleportPoint(int32 TeamId, const ADRTeleportPoint* TeleportPoint) const;

private:
	UPROPERTY(Replicated)
	TArray<FDRTeamRegisteredTeleportPoint> TeamRegisteredTeleports;
#pragma endregion
};
