#include "DRSnowJoinComponent.h"

#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/UI/Loading/DRLoadingUIComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

namespace DRSnowSnapshotTransfer
{
	constexpr int32 ChunkByteSize = 8 * 1024;
	constexpr int32 InitialWindow = 8;
	constexpr int32 MinWindow = 2;
	constexpr int32 MaxWindow = 16;
	constexpr double ExpectedAckSeconds = 0.25;
	constexpr double AdjustmentInterval = 0.5;
	constexpr float SendCheckInterval = 0.05f;
	constexpr double ProgressTimeoutSeconds = 60.0;
	constexpr double ApplyTimeoutSeconds = 120.0;
	constexpr double TotalTimeoutSeconds = 300.0;

	uint64 MakeChunkKey(uint8 PayloadType, int32 ByteOffset)
	{
		return static_cast<uint64>(PayloadType) << 32 | static_cast<uint32>(ByteOffset);
	}
}

UDRSnowJoinComponent::UDRSnowJoinComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

APlayerController* UDRSnowJoinComponent::GetPlayerController() const
{
	return CastChecked<APlayerController>(GetOwner());
}

void UDRSnowJoinComponent::BeginPlay()
{
	Super::BeginPlay();
	AddTickPrerequisiteActor(GetOwner());
	if (GetPlayerController()->IsLocalController() && GetNetMode() == NM_Client && SnowJoinStartedAt < 0.0)
	{
		SnowJoinStartedAt = LastSnowJoinProgressAt = FPlatformTime::Seconds();
	}
}

void UDRSnowJoinComponent::RefreshLoadingScreen()
{
	if (UDRLoadingUIComponent* LoadingUI = GetOwner()->FindComponentByClass<UDRLoadingUIComponent>())
	{
		LoadingUI->RefreshLoadingScreen();
	}
}

void UDRSnowJoinComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	CheckSnowJoinTimeout();

	if (!GetPlayerController()->IsLocalController() || bSnowJoinFailed)
	{
		return;
	}

	if (SnowJoinLoadingPhase != EDRSnowJoinLoadingPhase::Complete &&
		PendingSnowSnapshotId == INDEX_NONE &&
		GetPlayerController()->GetStateName() == NAME_Playing)
	{
		const APawn* ControlledPawn = GetPlayerController()->GetPawn();
		if (IsValid(ControlledPawn) && ControlledPawn->IsLocallyControlled())
		{
			SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::Complete;
			SnowJoinStartedAt = -1.0;
			LogSnowJoinControlState(TEXT("ClientControlReady"));

		}
	}

}

void UDRSnowJoinComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetSnowJoinTransfer();
	Super::EndPlay(EndPlayReason);
}

void UDRSnowJoinComponent::ResetSnowJoinTransfer()
{
	ResetOutgoingSnowSnapshot();
	ResetPendingSnowSnapshot();
	SnowJoinStartedAt = LastSnowJoinProgressAt = -1.0;
	ExpectedAppliedSnowSnapshotId = INDEX_NONE;
	bSnowSnapshotTransferFinished = false;
}

void UDRSnowJoinComponent::ResetOutgoingSnowSnapshot()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SnowSnapshotSendTimer);
	}
	OutgoingSnowSnapshotId = INDEX_NONE;
	OutgoingSnowPayloadType = 0;
	OutgoingSnowByteOffset = 0;
	PendingSnowChunkAcks.Empty();
	OutgoingSnowVoxelSaveData.Empty();
	OutgoingSnowVolumeData.Empty();
}

void UDRSnowJoinComponent::ResetPendingSnowSnapshot()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SnowJoinSnapshotRetryTimer);
	}
	PendingSnowSnapshotId = INDEX_NONE;
	PendingSnowCheckpointSequence = 0;
	PendingSnowVoxelWorldName = NAME_None;
	PendingSnowVoxelSaveByteCount = PendingSnowOriginalVoxelSaveSize = 0;
	PendingSnowVolumeByteCount = PendingSnowOriginalSnowVolumeSize = 0;
	bPendingSnowSnapshotFinished = false;
	PendingSnowVoxelSaveData.Empty();
	PendingSnowVolumeData.Empty();
	BufferedSnowOperations.Empty();
}

void UDRSnowJoinComponent::CheckSnowJoinTimeout()
{
	if (SnowJoinStartedAt < 0.0 || bSnowJoinFailed)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const bool bWaitingForApply = bSnowSnapshotTransferFinished
		|| SnowJoinLoadingPhase == EDRSnowJoinLoadingPhase::ApplyingSnapshot;
	const double ProgressTimeout = bWaitingForApply
		? DRSnowSnapshotTransfer::ApplyTimeoutSeconds : DRSnowSnapshotTransfer::ProgressTimeoutSeconds;
	if (Now - SnowJoinStartedAt >= DRSnowSnapshotTransfer::TotalTimeoutSeconds
		|| Now - LastSnowJoinProgressAt >= ProgressTimeout)
	{
		FailSnowJoin(TEXT("SnapshotTimeout"));
	}
}

void UDRSnowJoinComponent::FailSnowJoin(const TCHAR* Reason)
{
	if (bSnowJoinFailed)
	{
		return;
	}
	bSnowJoinFailed = true;
	UE_LOG(LogTemp, Warning, TEXT("[JoinSnapshot] Failed PC=%s Authority=%d Id=%d Phase=%d Reason=%s"),
		*GetNameSafe(GetPlayerController()), GetOwner()->HasAuthority(), GetOwner()->HasAuthority() ? ExpectedAppliedSnowSnapshotId : PendingSnowSnapshotId,
		static_cast<int32>(SnowJoinLoadingPhase), Reason);
	ResetSnowJoinTransfer();
	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::Failed;
	RefreshLoadingScreen();
	const FText Message = NSLOCTEXT("DRSnowJoin", "Failed", "중도 접속에 실패했습니다. 다시 접속해 주세요.");
	if (GetOwner()->HasAuthority())
	{
		ADRMiningGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>() : nullptr;
		if (GameMode)
		{
			GameMode->HandleSnowJoinSnapshotFailed(GetPlayerController());
		}
		GetPlayerController()->ClientReturnToMainMenuWithTextReason(Message);
		if (!GetPlayerController()->IsLocalController())
		{
			// 엔진의 Destroy -> Logout -> PlayerState 정리 경로를 사용한다.
			if (!GameMode || !GameMode->GameSession || !GameMode->GameSession->KickPlayer(GetPlayerController(), Message))
			{
				GetPlayerController()->Destroy();
			}
		}
	}
	else
	{
		ServerNotifySnowJoinSnapshotFailed();
		// 오류 통지 RPC가 끊긴 연결에 막혀도 클라이언트는 메뉴로 복귀한다.
		GetPlayerController()->ClientReturnToMainMenuWithTextReason(Message);
	}
}

void UDRSnowJoinComponent::ServerNotifySnowJoinSnapshotFailed_Implementation()
{
	FailSnowJoin(TEXT("ClientSnapshotFailed"));
}

void UDRSnowJoinComponent::BeginSnowJoinSnapshot(FDRSnowJoinCheckpoint&& Checkpoint)
{
	check(GetOwner()->HasAuthority());
	if (!Checkpoint.IsValid() || bSnowJoinFailed || ExpectedAppliedSnowSnapshotId != INDEX_NONE)
	{
		FailSnowJoin(TEXT("InvalidCheckpoint"));
		return;
	}
	ExpectedAppliedSnowSnapshotId = Checkpoint.SnapshotId;
	OutgoingSnowVoxelSaveData = MoveTemp(Checkpoint.VoxelSaveData);
	OutgoingSnowVolumeData = MoveTemp(Checkpoint.SnowVolumeData);
	SnowJoinStartedAt = LastSnowJoinProgressAt = FPlatformTime::Seconds();
	Client_BeginSnowJoinSnapshot(Checkpoint.SnapshotId, Checkpoint.OperationSequence, Checkpoint.VoxelWorldName,
		OutgoingSnowVoxelSaveData.Num(), Checkpoint.OriginalVoxelSaveSize,
		OutgoingSnowVolumeData.Num(), Checkpoint.OriginalSnowVolumeSize);
}

float UDRSnowJoinComponent::GetSnowJoinSnapshotProgress() const
{
	if (PendingSnowSnapshotId == INDEX_NONE)
	{
		return 1.f;
	}

	const int64 TotalByteCount =
		static_cast<int64>(PendingSnowVoxelSaveByteCount) +
		PendingSnowVolumeByteCount;
	if (TotalByteCount <= 0)
	{
		return 0.f;
	}

	const int64 ReceivedByteCount =
		static_cast<int64>(PendingSnowVoxelSaveData.Num()) +
		PendingSnowVolumeData.Num();
	return static_cast<float>(FMath::Clamp(
		static_cast<double>(ReceivedByteCount) / TotalByteCount,
		0.0,
		1.0));
}

void UDRSnowJoinComponent::Client_BeginSnowJoinSnapshot_Implementation(
	int32 SnapshotId,
	int32 CheckpointSequence,
	FName VoxelWorldName,
	int32 VoxelSaveByteCount,
	int32 OriginalVoxelSaveSize,
	int32 SnowVolumeByteCount,
	int32 OriginalSnowVolumeSize)
{
	if (bSnowJoinFailed)
	{
		return;
	}
	if (SnapshotId <= 0 || VoxelSaveByteCount <= 0 || SnowVolumeByteCount <= 0
		|| OriginalVoxelSaveSize <= 0 || OriginalSnowVolumeSize <= 0)
	{
		FailSnowJoin(TEXT("InvalidSnapshotHeader"));
		return;
	}

	SnowJoinStartedAt = LastSnowJoinProgressAt = FPlatformTime::Seconds();
	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::ReceivingSnapshot;
	PendingSnowSnapshotId = SnapshotId;
	PendingSnowCheckpointSequence = CheckpointSequence;
	PendingSnowVoxelWorldName = VoxelWorldName;
	PendingSnowVoxelSaveByteCount = VoxelSaveByteCount;
	PendingSnowOriginalVoxelSaveSize = OriginalVoxelSaveSize;
	PendingSnowVolumeByteCount = SnowVolumeByteCount;
	PendingSnowOriginalSnowVolumeSize = OriginalSnowVolumeSize;
	bPendingSnowSnapshotFinished = false;
	PendingSnowVoxelSaveData.Empty(VoxelSaveByteCount);
	PendingSnowVolumeData.Empty(SnowVolumeByteCount);
	BufferedSnowOperations.Reset();

	RefreshLoadingScreen();
	ServerRequestSnowJoinSnapshotData(SnapshotId);
}

void UDRSnowJoinComponent::ServerRequestSnowJoinSnapshotData_Implementation(int32 SnapshotId)
{
	UWorld* World = GetWorld();
	if (bSnowJoinFailed || OutgoingSnowSnapshotId != INDEX_NONE || bSnowSnapshotTransferFinished)
	{
		return;
	}
	if (!IsValid(World) || SnapshotId <= 0 || SnapshotId != ExpectedAppliedSnowSnapshotId
		|| OutgoingSnowVoxelSaveData.IsEmpty() || OutgoingSnowVolumeData.IsEmpty())
	{
		FailSnowJoin(TEXT("InvalidSnapshotRequest"));
		return;
	}

	OutgoingSnowSnapshotId = SnapshotId;
	OutgoingSnowPayloadType = 0;
	OutgoingSnowByteOffset = 0;
	PendingSnowChunkAcks.Reset();
	SnowSnapshotWindow = DRSnowSnapshotTransfer::InitialWindow;
	FastSnowSnapshotAcks = 0;
	SnowSnapshotSaturationStart = -1.0;
	LastSnowSnapshotWindowChange = LastSnowJoinProgressAt = FPlatformTime::Seconds();

	// ACK가 없거나 연결이 포화된 동안에도 전송 재개 여부를 확인한다.
	World->GetTimerManager().SetTimer(
		SnowSnapshotSendTimer, this, &ThisClass::SendNextSnowJoinSnapshotChunk,
		DRSnowSnapshotTransfer::SendCheckInterval, true);
	SendNextSnowJoinSnapshotChunk();
}

void UDRSnowJoinComponent::AdjustSnowSnapshotWindow(bool bIncrease, const TCHAR* Reason)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - LastSnowSnapshotWindowChange < DRSnowSnapshotTransfer::AdjustmentInterval)
	{
		return;
	}

	const int32 NewWindow = FMath::Clamp(
		bIncrease ? SnowSnapshotWindow * 2 : SnowSnapshotWindow / 2,
		DRSnowSnapshotTransfer::MinWindow, DRSnowSnapshotTransfer::MaxWindow);
	if (NewWindow == SnowSnapshotWindow)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[JoinSnapshot] Window Id=%d %d->%d Reason=%s Pending=%d"),
		OutgoingSnowSnapshotId, SnowSnapshotWindow, NewWindow, Reason, PendingSnowChunkAcks.Num());
	SnowSnapshotWindow = NewWindow;
	FastSnowSnapshotAcks = 0;
	LastSnowSnapshotWindowChange = Now;
}

void UDRSnowJoinComponent::SendNextSnowJoinSnapshotChunk()
{
	if (OutgoingSnowSnapshotId == INDEX_NONE)
	{
		return;
	}

	UNetConnection* Connection = GetPlayerController()->GetNetConnection();
	const double Now = FPlatformTime::Seconds();
	if (Connection != nullptr && !Connection->IsNetReady())
	{
		FastSnowSnapshotAcks = 0;
		if (SnowSnapshotSaturationStart < 0.0)
		{
			SnowSnapshotSaturationStart = Now;
		}
		if (Now - SnowSnapshotSaturationStart >= DRSnowSnapshotTransfer::AdjustmentInterval)
		{
			AdjustSnowSnapshotWindow(false, TEXT("Saturated"));
		}
	}
	else
	{
		SnowSnapshotSaturationStart = -1.0;
	}

	while (OutgoingSnowSnapshotId != INDEX_NONE
		&& PendingSnowChunkAcks.Num() < SnowSnapshotWindow)
	{
		const TArray<uint8>* Payload = nullptr;
		switch (OutgoingSnowPayloadType)
		{
		case 0:
			Payload = &OutgoingSnowVoxelSaveData;
			break;
		case 1:
			Payload = &OutgoingSnowVolumeData;
			break;
		default:
			if (PendingSnowChunkAcks.IsEmpty())
			{
				FinishSnowJoinSnapshotTransfer();
			}
			return;
		}

		if (OutgoingSnowByteOffset >= Payload->Num())
		{
			++OutgoingSnowPayloadType;
			OutgoingSnowByteOffset = 0;
			continue;
		}

		// Reliable RPC로 포화 상태를 더 악화시키지 않는다. 타이머가 다시 확인한다.
		if (Connection != nullptr && !Connection->IsNetReady())
		{
			return;
		}

		const int32 ChunkSize = FMath::Min(
			DRSnowSnapshotTransfer::ChunkByteSize,
			Payload->Num() - OutgoingSnowByteOffset);
		const uint8 SentPayloadType = OutgoingSnowPayloadType;
		const int32 SentByteOffset = OutgoingSnowByteOffset;
		TArray<uint8> ChunkData;
		ChunkData.Append(Payload->GetData() + SentByteOffset, ChunkSize);
		PendingSnowChunkAcks.Add(
			DRSnowSnapshotTransfer::MakeChunkKey(SentPayloadType, SentByteOffset),
			FPlatformTime::Seconds());
		OutgoingSnowByteOffset += ChunkSize;
		Client_ReceiveSnowJoinSnapshotChunk(
			OutgoingSnowSnapshotId,
			SentPayloadType,
			SentByteOffset,
			ChunkData);
	}
}

void UDRSnowJoinComponent::ServerAckSnowJoinSnapshotChunk_Implementation(
	int32 SnapshotId,
	uint8 PayloadType,
	int32 ByteOffset)
{
	if (SnapshotId != OutgoingSnowSnapshotId)
	{
		return;
	}

	const uint64 ChunkKey = DRSnowSnapshotTransfer::MakeChunkKey(PayloadType, ByteOffset);
	double SentTime = 0.0;
	if (!PendingSnowChunkAcks.RemoveAndCopyValue(ChunkKey, SentTime))
	{
		return;
	}

	LastSnowJoinProgressAt = FPlatformTime::Seconds();
	const double AckSeconds = LastSnowJoinProgressAt - SentTime;
	const UNetConnection* Connection = GetPlayerController()->GetNetConnection();
	if (AckSeconds >= DRSnowSnapshotTransfer::ExpectedAckSeconds)
	{
		FastSnowSnapshotAcks = 0;
		AdjustSnowSnapshotWindow(false, TEXT("SlowAck"));
	}
	else if (Connection == nullptr || Connection->IsNetReady())
	{
		// 한 윈도우 분량의 빠른 ACK가 이어질 때만 늘려 순간적인 왕복 시간 변동을 무시한다.
		FastSnowSnapshotAcks = FMath::Min(FastSnowSnapshotAcks + 1, SnowSnapshotWindow);
		if (FastSnowSnapshotAcks >= SnowSnapshotWindow)
		{
			AdjustSnowSnapshotWindow(true, TEXT("FastAck"));
		}
	}
	else
	{
		FastSnowSnapshotAcks = 0;
	}

	SendNextSnowJoinSnapshotChunk();
}

void UDRSnowJoinComponent::FinishSnowJoinSnapshotTransfer()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	LogSnowJoinControlState(TEXT("ServerTransferFinished"));
	World->GetTimerManager().ClearTimer(SnowSnapshotSendTimer);
	bSnowSnapshotTransferFinished = true;
	LastSnowJoinProgressAt = FPlatformTime::Seconds();
	Client_FinishSnowJoinSnapshot(OutgoingSnowSnapshotId);
	ResetOutgoingSnowSnapshot();
}

void UDRSnowJoinComponent::ServerNotifySnowJoinSnapshotApplied_Implementation(int32 SnapshotId)
{
	LogSnowJoinControlState(TEXT("ServerReceivedSnapshotApplied"));
	if (!bSnowSnapshotTransferFinished || SnapshotId != ExpectedAppliedSnowSnapshotId)
	{
		UE_LOG(LogTemp, Warning, TEXT("[JoinControl] ApplyNotifyRejected PC=%s ReceivedId=%d"),
			*GetNameSafe(GetPlayerController()), SnapshotId);
		return;
	}

	bSnowSnapshotTransferFinished = false;
	ExpectedAppliedSnowSnapshotId = INDEX_NONE;
	if (ADRMiningGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>() : nullptr)
	{
		if (!GameMode->HandleSnowJoinSnapshotApplied(GetPlayerController()))
		{
			FailSnowJoin(TEXT("PlayerSpawnFailed"));
			return;
		}
		SnowJoinStartedAt = -1.0;
		LogSnowJoinControlState(TEXT("ServerRestartPlayerReturned"));
	}
	else
	{
		FailSnowJoin(TEXT("MissingGameMode"));
	}

}

void UDRSnowJoinComponent::Client_ReceiveSnowJoinSnapshotChunk_Implementation(
	int32 SnapshotId,
	uint8 PayloadType,
	int32 ByteOffset,
	const TArray<uint8>& ChunkData)
{
	if (bSnowJoinFailed || SnapshotId != PendingSnowSnapshotId)
	{
		return;
	}

	TArray<uint8>* TargetData = nullptr;
	int32 ExpectedByteCount = 0;
	switch (PayloadType)
	{
	case 0:
		TargetData = &PendingSnowVoxelSaveData;
		ExpectedByteCount = PendingSnowVoxelSaveByteCount;
		break;
	case 1:
		TargetData = &PendingSnowVolumeData;
		ExpectedByteCount = PendingSnowVolumeByteCount;
		break;
	default:
		FailSnowJoin(TEXT("InvalidSnapshotPayload"));
		return;
	}
	// 동일 Controller의 Reliable RPC는 순서대로 도착한다. 빈 구간을 0으로 채우지 않는다.
	if (ByteOffset != TargetData->Num() || ChunkData.IsEmpty()
		|| ChunkData.Num() > DRSnowSnapshotTransfer::ChunkByteSize
		|| ByteOffset > ExpectedByteCount || ChunkData.Num() > ExpectedByteCount - ByteOffset)
	{
		FailSnowJoin(TEXT("InvalidSnapshotChunk"));
		return;
	}
	TargetData->Append(ChunkData);
	LastSnowJoinProgressAt = FPlatformTime::Seconds();
	ServerAckSnowJoinSnapshotChunk(SnapshotId, PayloadType, ByteOffset);
	TryApplyPendingSnowJoinSnapshot();
}

void UDRSnowJoinComponent::Client_FinishSnowJoinSnapshot_Implementation(int32 SnapshotId)
{
	if (SnapshotId != PendingSnowSnapshotId)
	{
		return;
	}

	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::ApplyingSnapshot;
	LastSnowJoinProgressAt = FPlatformTime::Seconds();
	if (PendingSnowVoxelSaveData.Num() != PendingSnowVoxelSaveByteCount
		|| PendingSnowVolumeData.Num() != PendingSnowVolumeByteCount)
	{
		FailSnowJoin(TEXT("IncompleteSnapshot"));
		return;
	}
	LogSnowJoinControlState(TEXT("ClientTransferFinished"));
	bPendingSnowSnapshotFinished = true;
	TryApplyPendingSnowJoinSnapshot();
}

bool UDRSnowJoinComponent::QueueSnowJoinOperation(const FDRSnowOperationRecord& Record)
{
	if (bSnowJoinFailed)
	{
		return true;
	}
	if (PendingSnowSnapshotId == INDEX_NONE)
	{
		return false;
	}
	if (Record.Sequence <= PendingSnowCheckpointSequence)
	{
		// 체크포인트에 포함된 작업은 스냅샷 위에 중복 적용하지 않는다.
		return true;
	}

	BufferedSnowOperations.Add(Record);
	return true;
}

void UDRSnowJoinComponent::TryApplyPendingSnowJoinSnapshot()
{
	if (PendingSnowSnapshotId == INDEX_NONE || !bPendingSnowSnapshotFinished ||
		PendingSnowVoxelSaveData.Num() != PendingSnowVoxelSaveByteCount ||
		PendingSnowVolumeData.Num() != PendingSnowVolumeByteCount)
	{
		return;
	}

	UWorld* World = GetWorld();
	UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	ADRMiningGameStateBase* MiningGameState = IsValid(World)
		? World->GetGameState<ADRMiningGameStateBase>() : nullptr;
	bool bWaitingForWorld = false;
	const bool bDependenciesReady = IsValid(MiningGameState) && IsValid(SnowSubsystem);
	if (!bDependenciesReady || !SnowSubsystem->ApplyCheckpoint(
		PendingSnowVoxelWorldName,
		PendingSnowVoxelSaveData,
		PendingSnowOriginalVoxelSaveSize,
		PendingSnowVolumeData,
		PendingSnowOriginalSnowVolumeSize, &bWaitingForWorld))
	{
		if (bDependenciesReady && !bWaitingForWorld)
		{
			FailSnowJoin(TEXT("SnapshotApplyFailed"));
			return;
		}
		if (IsValid(World))
		{
			World->GetTimerManager().SetTimer(
				SnowJoinSnapshotRetryTimer,
				this,
				&ThisClass::TryApplyPendingSnowJoinSnapshot,
				0.25f,
				false);
		}
		return;
	}

	MiningGameState->ResetSnowApplicationStateForCheckpoint(PendingSnowCheckpointSequence);
	World->GetTimerManager().ClearTimer(SnowJoinSnapshotRetryTimer);
	// 이벤트 콜백에서 재진입해도 같은 스냅샷을 다시 적용하지 않는다.
	bPendingSnowSnapshotFinished = false;
	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::WaitingForControl;
	LastSnowJoinProgressAt = FPlatformTime::Seconds();
	LogSnowJoinControlState(TEXT("ClientSnapshotApplied"));
	OnSnowJoinSnapshotApplied.Broadcast(PendingSnowSnapshotId);
	ServerNotifySnowJoinSnapshotApplied(PendingSnowSnapshotId);

	// Voxel 작업은 Pawn 스폰이나 서버의 추가 응답을 기다릴 필요가 없다.
	LogSnowJoinControlState(TEXT("ClientOperationsResumed"));
	TMap<int32, FDRSnowOperationRecord> OperationsBySequence;
	for (const FDRSnowOperationRecord& Record : BufferedSnowOperations)
	{
		if (Record.Sequence > PendingSnowCheckpointSequence)
		{
			OperationsBySequence.Add(Record.Sequence, Record);
		}
	}

	TArray<FDRSnowOperationRecord> Operations;
	OperationsBySequence.GenerateValueArray(Operations);
	Operations.Sort([](const FDRSnowOperationRecord& A, const FDRSnowOperationRecord& B)
	{
		return A.Sequence < B.Sequence;
	});

	const int32 AppliedSnapshotId = PendingSnowSnapshotId;
	const int32 AppliedVoxelSaveByteCount = PendingSnowVoxelSaveByteCount;
	const int32 AppliedSnowVolumeByteCount = PendingSnowVolumeByteCount;
	ResetPendingSnowSnapshot();
	// 조인 버퍼링을 해제한 뒤 전달해야 같은 작업을 다시 버퍼에 넣지 않는다.
	if (IsValid(MiningGameState))
	{
		for (const FDRSnowOperationRecord& Record : Operations)
		{
			MiningGameState->ApplySnowOperationRecord(Record);
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[JoinSnapshot] Applied Id=%d Voxel=%d bytes SnowVolume=%d bytes RecentOperations=%d"),
		AppliedSnapshotId,
		AppliedVoxelSaveByteCount,
		AppliedSnowVolumeByteCount,
		Operations.Num());
}

void UDRSnowJoinComponent::LogSnowJoinControlState(const TCHAR* Stage) const
{
	// 서버/클라이언트 로그의 시각을 대조해 적용, Pawn 복제, 소유 확인 지연을 구분한다.
	const APawn* ControlledPawn = GetPlayerController()->GetPawn();
	const UNetConnection* Connection = GetPlayerController()->GetNetConnection();
	UE_LOG(LogTemp, Log,
		TEXT("[JoinControl] Stage=%s PC=%s Authority=%d State=%s PendingId=%d ExpectedId=%d ")
		TEXT("Pawn=%s PawnController=%s AckPawn=%s LocalPawn=%d ")
		TEXT("HasConnection=%d NetReady=%d QueuedBits=%d NetSpeed=%d"),
		Stage, *GetNameSafe(GetPlayerController()), GetOwner()->HasAuthority(), *GetPlayerController()->GetStateName().ToString(),
		PendingSnowSnapshotId, ExpectedAppliedSnowSnapshotId, *GetNameSafe(ControlledPawn),
		*GetNameSafe(IsValid(ControlledPawn) ? ControlledPawn->GetController() : nullptr),
		*GetNameSafe(GetPlayerController()->AcknowledgedPawn), IsValid(ControlledPawn) && ControlledPawn->IsLocallyControlled(),
		Connection != nullptr, Connection != nullptr && Connection->IsNetReady(),
		Connection != nullptr ? Connection->QueuedBits : 0,
		Connection != nullptr ? Connection->CurrentNetSpeed : 0);
}
