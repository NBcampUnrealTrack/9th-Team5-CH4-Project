#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRSnowJoinComponent.generated.h"

class APlayerController;
struct FDRSnowJoinCheckpoint;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRSnowJoinSnapshotApplied, int32, SnapshotId);

UENUM(BlueprintType)
enum class EDRSnowJoinLoadingPhase : uint8
{
	Idle,
	ReceivingSnapshot,
	ApplyingSnapshot,
	WaitingForControl,
	Complete,
	Failed
};

// PlayerController의 기본 복제 컴포넌트. JIP RPC와 상태는 이 컴포넌트가 소유한다.
UCLASS(BlueprintType, ClassGroup = (Player))
class DEEPRAIDERS_API UDRSnowJoinComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSnowJoinComponent();
	// 소유/복제 생명주기 로그만 컨트롤러에서 전달한다.
	void LogSnowJoinControlState(const TCHAR* Stage) const;

	UFUNCTION(BlueprintPure, Category = "Snow|Join Snapshot")
	EDRSnowJoinLoadingPhase GetSnowJoinLoadingPhase() const { return SnowJoinLoadingPhase; }

	/** 스냅샷 네트워크 수신 진행률. 지형 적용/조종 준비 완료 여부는 Phase로 확인한다. */
	UFUNCTION(BlueprintPure, Category = "Snow|Join Snapshot")
	float GetSnowJoinSnapshotProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Snow|Join Snapshot")
	FDRSnowJoinSnapshotApplied OnSnowJoinSnapshotApplied;

	// 서버가 전송할 데이터를 소유하여 checkpoint 캐시 교체와 무관하게 유지한다.
	void BeginSnowJoinSnapshot(FDRSnowJoinCheckpoint&& Checkpoint);
	void FailSnowJoin(const TCHAR* Reason);

	UFUNCTION(Client, Reliable)
	void Client_BeginSnowJoinSnapshot(
		int32 SnapshotId,
		int32 CheckpointSequence,
		FName VoxelWorldName,
		int32 VoxelSaveByteCount,
		int32 OriginalVoxelSaveSize,
		int32 SnowVolumeByteCount,
		int32 OriginalSnowVolumeSize);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveSnowJoinSnapshotChunk(
		int32 SnapshotId,
		uint8 PayloadType,
		int32 ByteOffset,
		const TArray<uint8>& ChunkData);

	UFUNCTION(Client, Reliable)
	void Client_FinishSnowJoinSnapshot(int32 SnapshotId);

	// GameState multicast가 snapshot 적용 전에 도착하면 여기서 보관한다.
	bool QueueSnowJoinOperation(const FDRSnowOperationRecord& Record);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	APlayerController* GetPlayerController() const;
	void RefreshLoadingScreen();

	UFUNCTION(Server, Reliable)
	void ServerRequestSnowJoinSnapshotData(int32 SnapshotId);

	UFUNCTION(Server, Reliable)
	void ServerAckSnowJoinSnapshotChunk(
		int32 SnapshotId,
		uint8 PayloadType,
		int32 ByteOffset);

	UFUNCTION(Server, Reliable)
	void ServerNotifySnowJoinSnapshotApplied(int32 SnapshotId);

	UFUNCTION(Server, Reliable)
	void ServerNotifySnowJoinSnapshotFailed();

	void ResetSnowJoinTransfer();
	void ResetOutgoingSnowSnapshot();
	void ResetPendingSnowSnapshot();
	friend class FDRSnowJoinLifecycleTest;
	void CheckSnowJoinTimeout();
	bool bSnowJoinFailed = false;
	double SnowJoinStartedAt = -1.0;
	double LastSnowJoinProgressAt = -1.0;

	void SendNextSnowJoinSnapshotChunk();
	void AdjustSnowSnapshotWindow(bool bIncrease, const TCHAR* Reason);
	void FinishSnowJoinSnapshotTransfer();
	void TryApplyPendingSnowJoinSnapshot();

	int32 OutgoingSnowSnapshotId = INDEX_NONE;
	uint8 OutgoingSnowPayloadType = 0;
	int32 OutgoingSnowByteOffset = 0;
	TArray<uint8> OutgoingSnowVoxelSaveData;
	TArray<uint8> OutgoingSnowVolumeData;
	// 전송 시각으로 ACK 왕복 시간을 측정하고 동시 전송 수를 2~16개로 조절한다.
	TMap<uint64, double> PendingSnowChunkAcks;
	int32 SnowSnapshotWindow = 8;
	int32 FastSnowSnapshotAcks = 0;
	double SnowSnapshotSaturationStart = -1.0;
	double LastSnowSnapshotWindowChange = 0.0;
	FTimerHandle SnowSnapshotSendTimer;
	int32 ExpectedAppliedSnowSnapshotId = INDEX_NONE;
	bool bSnowSnapshotTransferFinished = false;

	int32 PendingSnowSnapshotId = INDEX_NONE;
	int32 PendingSnowCheckpointSequence = 0;
	FName PendingSnowVoxelWorldName;
	int32 PendingSnowVoxelSaveByteCount = 0;
	int32 PendingSnowOriginalVoxelSaveSize = 0;
	int32 PendingSnowVolumeByteCount = 0;
	int32 PendingSnowOriginalSnowVolumeSize = 0;
	bool bPendingSnowSnapshotFinished = false;
	TArray<uint8> PendingSnowVoxelSaveData;
	TArray<uint8> PendingSnowVolumeData;
	EDRSnowJoinLoadingPhase SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::Idle;
	TArray<FDRSnowOperationRecord> BufferedSnowOperations;
	FTimerHandle SnowJoinSnapshotRetryTimer;
};
