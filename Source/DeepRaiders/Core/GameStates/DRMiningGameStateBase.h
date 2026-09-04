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
	void RegisterSnowAdd(const FDRSnowAddOperation& Operation);
	void RegisterSnowRemove(const FDRSnowRemoveOperation& Operation);
	int32 GetSnowOperationSequence() const { return NextSnowOperationSequence; }
	void ResetSnowOperationState();
	void ResetSnowApplicationStateForCheckpoint(int32 CheckpointSequence);
	bool ApplySnowOperationRecord(const FDRSnowOperationRecord& Record);

	/** 새 경기를 위해 서버와 모든 클라이언트의 복셀 상태를 함께 초기화한다. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ResetVoxelState();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplySnowOperations(const TArray<FDRSnowOperationRecord>& Records);

private:
	void QueueSnowOperationForBroadcast(const FDRSnowOperationRecord& Record);
	void FlushSnowOperationBroadcasts();
	bool ApplySnowAddOnce(const FDRSnowAddOperation& Operation);
	bool ApplySnowRemoveOnce(const FDRSnowRemoveOperation& Operation);
	bool IsSnowOperationReady(const FDRSnowOperationRecord& Record) const;
	bool IsSnowOperationApplied(int32 Sequence) const;
	bool HasPendingSnowOperation(int32 Sequence) const;
	void QueuePendingSnowOperation(const FDRSnowOperationRecord& Record);
	void TryApplyPendingSnowOperations();
	void StartPendingSnowRetry();
	void StopPendingSnowRetry();
	AVoxelWorld* ResolveVoxelWorldByName(FName VoxelWorldName) const;

	int32 NextSnowOperationSequence = 0;
	int32 AppliedSnowCheckpointSequence = 0;
	TSet<int32> AppliedSnowOperationSequences;
	TArray<FDRSnowOperationRecord> PendingSnowOperations;
	TArray<FDRSnowOperationRecord> PendingSnowBroadcastOperations;
	FTimerHandle PendingSnowRetryTimer;
	FTimerHandle SnowOperationBroadcastTimer;
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
