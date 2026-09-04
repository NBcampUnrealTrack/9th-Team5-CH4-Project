#pragma once

#include "CoreMinimal.h"
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

USTRUCT()
struct FDRTeamRegisteredTeleportPoint
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TeamId = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<ADRTeleportPoint> TeleportPoint;
};

UCLASS()
class DEEPRAIDERS_API ADRMiningGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetGameTimerState(int32 RemainingSeconds, bool bStarted, bool bEnded);
	void SetGamePhaseState(
		int32 PhaseIndex,
		int32 RemainingSeconds,
		const TArray<FText>& PlayerMessages);
	void SetGameEndDebugText(const FString& DebugText);
	void SetGameResultText(const FText& ResultText);

	int32 GetGameRemainingSeconds() const { return GameRemainingSeconds; }
	bool IsGameStarted() const { return bGameStarted; }
	bool IsGameEnded() const { return bGameEnded; }
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
	void OnRep_GameTimerState();

	UFUNCTION()
	void OnRep_GameEndDebugText();

	UFUNCTION()
	void OnRep_GameResultText();

	UFUNCTION()
	void OnRep_GamePhaseState();

	UPROPERTY(ReplicatedUsing = OnRep_GameTimerState)
	int32 GameRemainingSeconds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_GameTimerState)
	bool bGameStarted = false;

	UPROPERTY(ReplicatedUsing = OnRep_GameTimerState)
	bool bGameEnded = false;

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
	void RegisterSnowAdd(
		const FDRSnowAddOperation& Operation,
		float ServerAppliedAmount = 0.f);
	void RegisterSnowRemove(
		const FDRSnowRemoveOperation& Operation,
		FDRSnowMaterialPatch MaterialPatch);
	int32 GetSnowOperationSequence() const { return NextSnowOperationSequence; }
	void ResetSnowOperationState();
	void ResetSnowApplicationStateForCheckpoint(int32 CheckpointSequence);
	bool ApplySnowOperationRecord(const FDRSnowOperationRecord& Record);

	// 새 경기를 시작할 때 서버와 모든 클라이언트의 복셀/지형 데이터를 초기화합니다.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ResetVoxelState();

	// 짧은 시간 동안 발생한 눈 변경 작업들을 배열로 묶어 한 번에 보내는 Reliable Multicast RPC
	// 패킷 내 배열 순서는 서버의 발생 순서(Sequence)와 동일합니다.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplySnowOperations(const TArray<FDRSnowOperationRecord>& Records);

private:
	// 눈 작업 배치 전송 시스템 (서버 전용)
	// 매 작업마다 RPC를 보내면 연사나 산탄 시 패킷 폭주가 발생하므로,
	// 대기열에 모아두었다가 제한된 크기의 배치로 나누어 전송합니다.

	// 작업을 대기열에 추가하고 배치 전송 타이머를 예약합니다.
	void QueueSnowOperationForBroadcast(FDRSnowOperationRecord&& Record);

	// 타이머가 꺼져 있을 때만 다음 배치 전송을 예약합니다.
	void ScheduleSnowOperationBroadcast();

	// 대기열의 작업들을 분리하여 Multicast_ApplySnowOperations RPC로 일괄 발송합니다.
	void FlushSnowOperationBroadcasts();

	// 미전송 대기열과 타이머를 취소합니다 (게임 종료 및 상태 초기화 시 호출).
	void ClearSnowOperationBroadcasts();

	bool ApplySnowAddOnce(const FDRSnowOperationRecord& Record);
	bool ApplySnowRemoveOnce(const FDRSnowOperationRecord& Record);
	bool IsSnowOperationReady(const FDRSnowOperationRecord& Record) const;
	bool IsSnowOperationApplied(int32 Sequence) const;
	bool HasPendingSnowOperation(int32 Sequence) const;
	void QueuePendingSnowOperation(const FDRSnowOperationRecord& Record);
	void TryApplyPendingSnowOperations();
	void StartPendingSnowRetry();
	void StopPendingSnowRetry();
	AVoxelWorld* ResolveVoxelWorldByName(FName VoxelWorldName) const;

	// 서버 전송용: 아직 클라이언트로 전송되지 않은 눈 작업 묶음 대기열
	TArray<FDRSnowOperationRecord> PendingSnowBroadcastOperations;
	FTimerHandle SnowOperationBroadcastTimer;

	// 공통: 눈 작업 고유 번호 (서버: 순차 발급, 클라이언트: 중복 처리 방지용)
	int32 NextSnowOperationSequence = 0;

	// 클라이언트: 체크포인트 스냅샷으로 이미 처리 완료된 작업 번호 기준선
	int32 AppliedSnowCheckpointSequence = 0;

	// 클라이언트: 이미 로컬에 반영 완료된 작업 번호 목록 (중복 실행 방지)
	TSet<int32> AppliedSnowOperationSequences;

	// 클라이언트: 복셀 월드가 아직 로드되지 않아 생성을 기다리는 작업 목록
	TArray<FDRSnowOperationRecord> PendingSnowOperations;
	FTimerHandle PendingSnowRetryTimer;
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
