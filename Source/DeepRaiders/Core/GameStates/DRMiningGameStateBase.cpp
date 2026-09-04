#include "DRMiningGameStateBase.h"

#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Gameplay/Voxel/DRMeshVoxelCarver.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Teleport/DRTeleportPoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"

void ADRMiningGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSnowOperationBroadcasts();
	StopPendingSnowRetry();
	PendingSnowOperations.Reset();
	AppliedSnowOperationSequences.Reset();

	Super::EndPlay(EndPlayReason);
}

void ADRMiningGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRMiningGameStateBase, TeamRegisteredTeleports);
	DOREPLIFETIME(ADRMiningGameStateBase, GameRemainingSeconds);
	DOREPLIFETIME(ADRMiningGameStateBase, bGameStarted);
	DOREPLIFETIME(ADRMiningGameStateBase, bGameEnded);
	DOREPLIFETIME(ADRMiningGameStateBase, GameEndDebugText);
	DOREPLIFETIME(ADRMiningGameStateBase, GameResultText);
	DOREPLIFETIME(ADRMiningGameStateBase, CurrentPhaseIndex);
	DOREPLIFETIME(ADRMiningGameStateBase, PhaseRemainingSeconds);
	DOREPLIFETIME(ADRMiningGameStateBase, CurrentPhaseMessages);
}

void ADRMiningGameStateBase::SetGameTimerState(int32 RemainingSeconds, bool bStarted, bool bEnded)
{
	if (!HasAuthority())
	{
		return;
	}

	GameRemainingSeconds = FMath::Max(0, RemainingSeconds);
	bGameStarted = bStarted;
	bGameEnded = bEnded;
	OnRep_GameTimerState();
	ForceNetUpdate();
}

void ADRMiningGameStateBase::OnRep_GameTimerState()
{
	OnGameTimerChanged.Broadcast(GameRemainingSeconds, bGameStarted, bGameEnded);
}

void ADRMiningGameStateBase::SetGamePhaseState(
	int32 PhaseIndex,
	int32 RemainingSeconds,
	const TArray<FText>& PlayerMessages)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentPhaseIndex = PhaseIndex;
	PhaseRemainingSeconds = FMath::Max(0, RemainingSeconds);
	CurrentPhaseMessages = PlayerMessages;
	OnRep_GamePhaseState();
	ForceNetUpdate();
}

void ADRMiningGameStateBase::OnRep_GamePhaseState()
{
	OnGamePhaseChanged.Broadcast(
		CurrentPhaseIndex,
		PhaseRemainingSeconds,
		CurrentPhaseMessages);
}

void ADRMiningGameStateBase::SetGameEndDebugText(const FString& DebugText)
{
	if (!HasAuthority())
	{
		return;
	}

	GameEndDebugText = DebugText;
	OnRep_GameEndDebugText();
	ForceNetUpdate();
}

void ADRMiningGameStateBase::OnRep_GameEndDebugText()
{
	OnGameEndDebugTextChanged.Broadcast(GameEndDebugText);
	UE_LOG(LogTemp, Warning, TEXT("[GameEnd][Replicated]\n%s"), *GameEndDebugText);
}

void ADRMiningGameStateBase::SetGameResultText(const FText& ResultText)
{
	if (!HasAuthority())
	{
		return;
	}

	GameResultText = ResultText;
	OnRep_GameResultText();
	ForceNetUpdate();
}

void ADRMiningGameStateBase::OnRep_GameResultText()
{
	OnGameResultTextChanged.Broadcast(GameResultText);
}

#pragma region Terrain Dig
void ADRMiningGameStateBase::RegisterTerrainDig(const FDRTerrainDigOperation& Operation)
{
	if (!HasAuthority())
	{
		return;
	}

	// 이미 접속 중인 클라이언트에게 서버 확정 지형 변경 이벤트만 전파한다.
	Multicast_ApplyTerrainDig(Operation);
}

void ADRMiningGameStateBase::Multicast_ApplyTerrainDig_Implementation(const FDRTerrainDigOperation& Operation)
{
	if (HasAuthority())
	{
		return;
	}

	ApplyTerrainDigOnce(Operation);
}

bool ADRMiningGameStateBase::ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	UDRVoxelTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(TerrainSubsystem))
	{
		return false;
	}

	// VoxelWorld 준비 여부와 pending 처리는 TerrainSubsystem 하나에서 관리한다.
	return TerrainSubsystem->ApplyOrQueueDig(Operation);
}
#pragma endregion

#pragma region Snow
void ADRMiningGameStateBase::RegisterSnowAdd(
	const FDRSnowAddOperation& Operation,
	const float ServerAppliedAmount)
{
	if (!HasAuthority())
	{
		return;
	}

	FDRSnowOperationRecord Record;
	Record.Sequence = ++NextSnowOperationSequence;
	Record.bIsAddOperation = true;
	Record.AddOperation = Operation;
	Record.ServerAppliedAmount = ServerAppliedAmount > 0.f
		? ServerAppliedAmount
		: Operation.Amount;
	QueueSnowOperationBroadcast(MoveTemp(Record));
}

void ADRMiningGameStateBase::RegisterSnowRemove(
	const FDRSnowRemoveOperation& Operation,
	FDRSnowMaterialPatch MaterialPatch)
{
	if (!HasAuthority())
	{
		return;
	}

	FDRSnowOperationRecord Record;
	Record.Sequence = ++NextSnowOperationSequence;
	Record.bIsAddOperation = false;
	Record.RemoveOperation = Operation;
	Record.ServerAppliedAmount = Operation.AppliedAmount;
	Record.bHasAuthoritativeMaterialPatch = true;
	Record.MaterialPatch = MoveTemp(MaterialPatch);
	QueueSnowOperationBroadcast(MoveTemp(Record));
}

void ADRMiningGameStateBase::ResetSnowOperationState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Sequence를 0으로 되돌리기 전에 이전 경기의 미전송 배치를 폐기한다.
	ClearSnowOperationBroadcasts();
	NextSnowOperationSequence = 0;
	AppliedSnowCheckpointSequence = 0;
	AppliedSnowOperationSequences.Reset();
	PendingSnowOperations.Reset();
	StopPendingSnowRetry();
}

void ADRMiningGameStateBase::ResetSnowApplicationStateForCheckpoint(int32 CheckpointSequence)
{
	if (HasAuthority())
	{
		return;
	}

	AppliedSnowCheckpointSequence = FMath::Max(0, CheckpointSequence);
	AppliedSnowOperationSequences.Reset();
	PendingSnowOperations.RemoveAll([this](const FDRSnowOperationRecord& Record)
	{
		return Record.Sequence > 0 && Record.Sequence <= AppliedSnowCheckpointSequence;
	});

	if (PendingSnowOperations.IsEmpty())
	{
		StopPendingSnowRetry();
	}
	else
	{
		StartPendingSnowRetry();
	}
}

void ADRMiningGameStateBase::Multicast_ResetVoxelState_Implementation()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
	{
		SnowSubsystem->ResetSnowState();
	}

	if (UDRVoxelTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>())
	{
		TerrainSubsystem->ResetTerrainState();
	}

	for (TActorIterator<AVoxelWorld> Iterator(World); Iterator; ++Iterator)
	{
		if (Iterator->IsCreated())
		{
			UVoxelBlueprintLibrary::ClearAllData(*Iterator, true);
		}
	}

	for (TActorIterator<ADRMeshVoxelCarver> Iterator(World); Iterator; ++Iterator)
	{
		Iterator->RestartCarveBatch();
	}

	if (HasAuthority())
	{
		ResetSnowOperationState();
		return;
	}

	AppliedSnowCheckpointSequence = 0;
	AppliedSnowOperationSequences.Reset();
	PendingSnowOperations.Reset();
	StopPendingSnowRetry();
}

void ADRMiningGameStateBase::QueueSnowOperationBroadcast(FDRSnowOperationRecord&& Record)
{
	if (!HasAuthority())
	{
		return;
	}

	PendingSnowBroadcastOperations.Add(MoveTemp(Record));

	// 산탄/연사로 한 프레임에 작업이 몰리면 30Hz 타이머를 기다리지 않고 상한에서 즉시 보낸다.
	if (PendingSnowBroadcastOperations.Num() >= MaxSnowOperationsPerBatch)
	{
		FlushSnowOperationBroadcasts();
		return;
	}

	ScheduleSnowOperationBroadcast();
}

void ADRMiningGameStateBase::ScheduleSnowOperationBroadcast()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsValid(World) || PendingSnowBroadcastOperations.IsEmpty() ||
		World->GetTimerManager().IsTimerActive(SnowOperationBroadcastTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		SnowOperationBroadcastTimer,
		this,
		&ThisClass::FlushSnowOperationBroadcasts,
		SnowOperationBroadcastInterval,
		false);
}

void ADRMiningGameStateBase::FlushSnowOperationBroadcasts()
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(SnowOperationBroadcastTimer);
	}
	SnowOperationBroadcastTimer.Invalidate();

	if (!HasAuthority() || PendingSnowBroadcastOperations.IsEmpty())
	{
		return;
	}

	// RPC 호출 중 새 작업이 등록되더라도 현재 배치와 섞이지 않도록 먼저 분리한다.
	TArray<FDRSnowOperationRecord> Batch = MoveTemp(PendingSnowBroadcastOperations);
	PendingSnowBroadcastOperations.Reset();

	Multicast_ApplySnowOperations(Batch);

	// 재진입 등으로 RPC 호출 중 새 작업이 들어왔다면 다음 30Hz 창을 예약한다.
	if (!PendingSnowBroadcastOperations.IsEmpty())
	{
		ScheduleSnowOperationBroadcast();
	}
}

void ADRMiningGameStateBase::ClearSnowOperationBroadcasts()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SnowOperationBroadcastTimer);
	}
	SnowOperationBroadcastTimer.Invalidate();
	PendingSnowBroadcastOperations.Reset();
}

void ADRMiningGameStateBase::Multicast_ApplySnowOperations_Implementation(
	const TArray<FDRSnowOperationRecord>& Records)
{
	if (HasAuthority() || Records.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	ADRPlayerController* PlayerController = IsValid(World)
		? Cast<ADRPlayerController>(World->GetFirstPlayerController())
		: nullptr;

	// Reliable RPC 내부 배열 순서는 서버 Sequence 생성 순서와 동일하다.
	// Join snapshot 중이면 각 Record를 기존 QueueSnowJoinOperation 경로로 넘겨
	// checkpoint/history 중복 제거 규칙을 그대로 유지한다.
	for (const FDRSnowOperationRecord& Record : Records)
	{
		if (IsValid(PlayerController) && PlayerController->QueueSnowJoinOperation(Record))
		{
			continue;
		}

		ApplySnowOperationRecord(Record);
	}
}

bool ADRMiningGameStateBase::ApplySnowOperationRecord(const FDRSnowOperationRecord& Record)
{
	if (IsSnowOperationApplied(Record.Sequence))
	{
		return true;
	}

	// 먼저 도착한 작업이 VoxelWorld 생성을 기다리고 있으면 이후 작업도 큐에
	// 넣어 서버 Sequence 순서를 유지한다.
	if (!PendingSnowOperations.IsEmpty() || !IsSnowOperationReady(Record))
	{
		QueuePendingSnowOperation(Record);
		TryApplyPendingSnowOperations();
		return IsSnowOperationApplied(Record.Sequence);
	}

	const bool bChanged = Record.bIsAddOperation
		? ApplySnowAddOnce(Record)
		: ApplySnowRemoveOnce(Record);

	// 준비된 상태에서 한 번 실행한 작업은 변경량이 0이어도 소비한다.
	// 재시도하면 비멱등 눈 작업이 중복 적용될 수 있다.
	if (Record.Sequence > 0)
	{
		AppliedSnowOperationSequences.Add(Record.Sequence);
	}

	return bChanged;
}

bool ADRMiningGameStateBase::IsSnowOperationReady(const FDRSnowOperationRecord& Record) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(World->GetSubsystem<UDRSnowSubsystem>()))
	{
		return false;
	}

	const FName VoxelWorldName = Record.bIsAddOperation
		? Record.AddOperation.VoxelWorldName
		: Record.RemoveOperation.VoxelWorldName;
	AVoxelWorld* VoxelWorld = ResolveVoxelWorldByName(VoxelWorldName);
	return IsValid(VoxelWorld) && VoxelWorld->IsCreated();
}

bool ADRMiningGameStateBase::IsSnowOperationApplied(int32 Sequence) const
{
	return Sequence > 0 &&
		(Sequence <= AppliedSnowCheckpointSequence || AppliedSnowOperationSequences.Contains(Sequence));
}

bool ADRMiningGameStateBase::HasPendingSnowOperation(int32 Sequence) const
{
	if (Sequence <= 0)
	{
		return false;
	}

	return PendingSnowOperations.ContainsByPredicate([Sequence](const FDRSnowOperationRecord& Record)
	{
		return Record.Sequence == Sequence;
	});
}

void ADRMiningGameStateBase::QueuePendingSnowOperation(const FDRSnowOperationRecord& Record)
{
	if (IsSnowOperationApplied(Record.Sequence) || HasPendingSnowOperation(Record.Sequence))
	{
		return;
	}

	PendingSnowOperations.Add(Record);
	PendingSnowOperations.Sort([](const FDRSnowOperationRecord& A, const FDRSnowOperationRecord& B)
	{
		return A.Sequence < B.Sequence;
	});
	StartPendingSnowRetry();
}

void ADRMiningGameStateBase::TryApplyPendingSnowOperations()
{
	while (!PendingSnowOperations.IsEmpty())
	{
		const FDRSnowOperationRecord Record = PendingSnowOperations[0];
		if (IsSnowOperationApplied(Record.Sequence))
		{
			PendingSnowOperations.RemoveAt(0);
			continue;
		}

		if (!IsSnowOperationReady(Record))
		{
			break;
		}

		if (Record.bIsAddOperation)
		{
			ApplySnowAddOnce(Record);
		}
		else
		{
			ApplySnowRemoveOnce(Record);
		}
		if (Record.Sequence > 0)
		{
			AppliedSnowOperationSequences.Add(Record.Sequence);
		}
		PendingSnowOperations.RemoveAt(0);
	}

	if (PendingSnowOperations.IsEmpty())
	{
		StopPendingSnowRetry();
	}
}

void ADRMiningGameStateBase::StartPendingSnowRetry()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetTimerManager().IsTimerActive(PendingSnowRetryTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PendingSnowRetryTimer,
		this,
		&ThisClass::TryApplyPendingSnowOperations,
		0.1f,
		true);
}

void ADRMiningGameStateBase::StopPendingSnowRetry()
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(PendingSnowRetryTimer);
	}
}

bool ADRMiningGameStateBase::ApplySnowAddOnce(const FDRSnowOperationRecord& Record)
{
	const FDRSnowAddOperation& Operation = Record.AddOperation;
	const bool bUsesOrientedBox = Operation.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool;
	if (Operation.Amount <= 0.f ||
		(bUsesOrientedBox && (Operation.BoxExtent.X <= 0.f || Operation.BoxExtent.Y <= 0.f || Operation.BoxExtent.Z <= 0.f)) ||
		(!bUsesOrientedBox && Operation.Radius <= 0.f))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorldByName(Operation.VoxelWorldName);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FDRSnowSurfaceAddRequest Request;
	Request.WorldLocation = Operation.WorldLocation;
	Request.SurfaceNormal = FVector(Operation.SurfaceNormal).IsNearlyZero()
		? FVector::UpVector
		: FVector(Operation.SurfaceNormal).GetSafeNormal();
	Request.ImpactDirection = FVector(Operation.ImpactDirection).IsNearlyZero()
		? -Request.SurfaceNormal
		: FVector(Operation.ImpactDirection).GetSafeNormal();
	Request.TargetVoxelWorld = VoxelWorld;
	Request.Radius = Operation.Radius;
	Request.Amount = Operation.Amount;
	Request.BoxExtent = Operation.BoxExtent;
	Request.BoxRotation = Operation.BoxRotation;
	Request.EditTool = Operation.EditTool;
	Request.bAllowVirtualSurfaceFallback = Operation.bAllowVirtualSurfaceFallback;
	Request.bUseVirtualSurface = Operation.bUseVirtualSurface;
	Request.Context.TeamId = Operation.TeamId;

	if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
	{
		const float ServerAppliedAmount = Record.ServerAppliedAmount > 0.f
			? Record.ServerAppliedAmount
			: Operation.Amount;
		return SnowSubsystem->ApplyReplicatedSnowAdd(Request, ServerAppliedAmount).AddedAmount > 0.f;
	}

	return false;
}

bool ADRMiningGameStateBase::ApplySnowRemoveOnce(const FDRSnowOperationRecord& Record)
{
	const FDRSnowRemoveOperation& Operation = Record.RemoveOperation;
	if (Operation.Radius <= 0.f || Operation.RequestedAmount <= 0.f ||
		Operation.AppliedAmount <= 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorldByName(Operation.VoxelWorldName);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FDRSnowSurfaceRemoveRequest Request;
	Request.WorldLocation = Operation.WorldLocation;
	Request.SurfaceNormal = FVector(Operation.SurfaceNormal).IsNearlyZero()
		? FVector::UpVector
		: FVector(Operation.SurfaceNormal).GetSafeNormal();
	Request.BrushOrigin = Operation.BrushOrigin;
	Request.TargetVoxelWorld = VoxelWorld;
	Request.Radius = Operation.Radius;
	Request.RequestedAmount = Operation.RequestedAmount;
	Request.RemovalBrushShape = Operation.RemovalBrushShape;
	Request.RemovalMode = Operation.RemovalMode;
	Request.AbsorbInnerRadiusRatio = Operation.AbsorbInnerRadiusRatio;
	Request.AbsorbSweepRadius = Operation.AbsorbSweepRadius;
	Request.AbsorbMaxSweepsPerTick = Operation.AbsorbMaxSweepsPerTick;
	Request.bUseAdaptiveAbsorbQuery = Operation.bUseAdaptiveAbsorbQuery;
	Request.Context.TeamId = Operation.TeamId;

	UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>();
	if (!IsValid(SnowSubsystem))
	{
		return false;
	}

	// 표면 처리의 재현 결과가 한 voxel 정도 달라도, 원본 점령 데이터는
	// 서버가 확정한 실제 제거량으로 동일하게 유지한다.
	const float ServerAppliedAmount = Record.ServerAppliedAmount > 0.f
		? Record.ServerAppliedAmount
		: Operation.AppliedAmount;
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch =
		Record.bHasAuthoritativeMaterialPatch ? &Record.MaterialPatch : nullptr;
	return Operation.RemovalMode == EDRSnowRemovalMode::AbsorbTool
		? SnowSubsystem->ApplyReplicatedSnowAbsorbTool(
			Request,
			ServerAppliedAmount,
			AuthoritativeMaterialPatch)
		: SnowSubsystem->ApplyReplicatedSnowRemoval(
			Request,
			ServerAppliedAmount,
			AuthoritativeMaterialPatch);
}

AVoxelWorld* ADRMiningGameStateBase::ResolveVoxelWorldByName(FName VoxelWorldName) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		AVoxelWorld* VoxelWorld = *It;
		if (!IsValid(VoxelWorld))
		{
			continue;
		}

		if (VoxelWorldName.IsNone() || VoxelWorld->GetFName() == VoxelWorldName)
		{
			return VoxelWorld;
		}
	}

	return nullptr;
}
#pragma endregion

#pragma region Teleport
void ADRMiningGameStateBase::AddTeamRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint)
{
	if (!HasAuthority() || TeamId == INDEX_NONE || !IsValid(TeleportPoint) || !TeleportPoint->IsRegisteredForTeam(TeamId))
	{
		return;
	}

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && RegisteredTeleport.TeleportPoint == TeleportPoint)
		{
			return;
		}
	}

	FDRTeamRegisteredTeleportPoint RegisteredTeleport;
	RegisteredTeleport.TeamId = TeamId;
	RegisteredTeleport.TeleportPoint = TeleportPoint;
	TeamRegisteredTeleports.Add(RegisteredTeleport);
	ForceNetUpdate();
}

void ADRMiningGameStateBase::RemoveRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint)
{
	if (!HasAuthority() || !IsValid(TeleportPoint))
	{
		return;
	}

	TeamRegisteredTeleports.RemoveAll([TeleportPoint](const FDRTeamRegisteredTeleportPoint& RegisteredTeleport)
	{
		return RegisteredTeleport.TeleportPoint == TeleportPoint;
	});

	ForceNetUpdate();
}

void ADRMiningGameStateBase::GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && IsValid(RegisteredTeleport.TeleportPoint))
		{
			OutTeleportPoints.AddUnique(RegisteredTeleport.TeleportPoint);
		}
	}
}

void ADRMiningGameStateBase::GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && IsValid(RegisteredTeleport.TeleportPoint))
		{
			OutTeleportPoints.AddUnique(RegisteredTeleport.TeleportPoint);
		}
	}
}

void ADRMiningGameStateBase::GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	GetRegisteredTeleportPointsForTeam(TeamId, OutTeleportPoints);
	OutTeleportPoints.Remove(CurrentTeleportPoint);
}

bool ADRMiningGameStateBase::CanTeamUseRegisteredTeleportPoint(int32 TeamId, const ADRTeleportPoint* TeleportPoint) const
{
	if (!IsValid(TeleportPoint) || !TeleportPoint->IsRegisteredForTeam(TeamId))
	{
		return false;
	}

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && RegisteredTeleport.TeleportPoint == TeleportPoint)
		{
			return true;
		}
	}

	return false;
}

#pragma endregion
