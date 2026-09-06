#include "DRMiningGameStateBase.h"

#include "DeepRaiders/Snow/DRSnowNetworkUtils.h"

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
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/IConsoleManager.h"
#include "Async/Async.h"
#include "CoreGlobals.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
TAutoConsoleVariable<int32> CVarDRSnowFastReplay(
	TEXT("dr.Snow.FastReplay"), 1, TEXT("Continue completed client edits via GT tasks; preserve sequence order."));
TAutoConsoleVariable<int32> CVarDRSnowReplayMaxStarts(
	TEXT("dr.Snow.ReplayMaxStartsPerFrame"), 4, TEXT("Client replay start limit per frame, 1..32."));
TAutoConsoleVariable<float> CVarDRSnowReplayBudgetMs(
	TEXT("dr.Snow.ReplayBudgetMs"), 4.f, TEXT("Soft budget for synchronous client replay dispatch per frame. One operation cannot be preempted."));

void LogSnowReplayPerf(const TCHAR* Stage, UWorld* World, int32 Sequence, int32 Generation, int32 Pending)
{
	if (!IsValid(World) || !FDRSnowSurfaceEditor::IsDirectionalPerfLoggingEnabled())
	{
		return;
	}
	// Same-process timestamps only. Receive -> Complete excludes network travel,
	// join-snapshot buffering, and asynchronous mesh/collision completion.
	UE_LOG(LogTemp, Log,
		TEXT("[DRSnowReplayPerf] Version=1.5 Stage=%s PID=%u Role=%s Mode=%s World=%s Sequence=%d Generation=%d Time=%.9f Pending=%d"),
		Stage, FPlatformProcess::GetCurrentProcessId(), World->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Server"),
		FDRSnowSurfaceEditor::IsCombinedDirectionalEditEnabled() ? TEXT("Combined") : TEXT("Legacy"),
		*World->GetName(), Sequence, Generation, FPlatformTime::Seconds(), Pending);
}
}

// Synthetic HIT-stage load: no fake players, GAS, bullets, or new RPCs.
// A TargetPoint with actor tag SnowPerfAnchor selects the test area.
#if !UE_BUILD_SHIPPING
struct FDRSnowLoadTest : TSharedFromThis<FDRSnowLoadTest>
{
	TWeakObjectPtr<ADRMiningGameStateBase> State;
	FTimerHandle Timer;
	FRandomStream Random{12345};
	int32 Shooters = 6, Pellets = 8, Bursts = 10, Burst = 0;
	int32 Submitted = 0, Completed = 0, Changed = 0, Misses = 0;
	float Interval = 1.f, Radius = 65.f, Amount = 1.5f, Warmup = 20.f;
	double StartTime = 0.0;
	FString Run;
	bool bActive = false;

	bool Configure(const TArray<FString>& Args)
	{
		if (Args.Num() != 0 && Args.Num() != 7) { return false; }
		if (Args.Num() == 7)
		{
			for (const FString& Arg : Args)
			{
				if (!Arg.IsNumeric())
				{
					return false;
				}
			}

			Shooters = FCString::Atoi(*Args[0]);
			Pellets = FCString::Atoi(*Args[1]);
			Interval = FCString::Atof(*Args[2]);
			Bursts = FCString::Atoi(*Args[3]);
			Radius = FCString::Atof(*Args[4]);
			Amount = FCString::Atof(*Args[5]);
			Warmup = FCString::Atof(*Args[6]);
		}
		return Shooters >= 1 && Shooters <= 6 && Pellets >= 1 && Pellets <= 8 &&
			Bursts >= 1 && Bursts <= 100 && FMath::IsFinite(Interval) && Interval >= 0.1f && Interval <= 10.f &&
			FMath::IsFinite(Radius) && Radius >= 1.f && Radius <= 280.f &&
			FMath::IsFinite(Amount) && Amount > 0.f && Amount <= 5.f &&
			FMath::IsFinite(Warmup) && Warmup >= 0.f && Warmup <= 120.f;
	}

	void Stop(const TCHAR* Reason)
	{
		if (!bActive) { return; }
		bActive = false;
		if (ADRMiningGameStateBase* Owner = State.Get())
		{
			Owner->GetWorldTimerManager().ClearTimer(Timer);
		}
		UE_LOG(LogTemp, Log, TEXT("[DRSnowLoad] Stage=End PID=%u Run=%s Reason=%s Bursts=%d Submitted=%d Completed=%d Changed=%d Misses=%d Outstanding=%d"),
			FPlatformProcess::GetCurrentProcessId(), *Run, Reason, Burst, Submitted, Completed, Changed, Misses, Submitted - Completed);
	}

	void Schedule()
	{
		ADRMiningGameStateBase* Owner = State.Get();
		if (!bActive || !IsValid(Owner)) { return; }
		TWeakPtr<FDRSnowLoadTest> Weak = AsShared();
		Timer = Owner->GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Weak]()
		{
			if (auto Test = Weak.Pin()) { Test->Tick(); }
		}));
	}

	void Tick()
	{
		ADRMiningGameStateBase* Owner = State.Get();
		if (!bActive || !IsValid(Owner)) { return; }
		UWorld* World = Owner->GetWorld();
		const double Now = FPlatformTime::Seconds();
		const double Deadline = StartTime + Warmup + Burst * Interval;
		if (Burst == Bursts)
		{
			if (Submitted == Completed) { Stop(TEXT("Drained")); return; }
			if (Now > Deadline + 30.0) { Stop(TEXT("DrainTimeout")); return; }
			Schedule(); return;
		}
		if (Now < Deadline) { Schedule(); return; }
		// Do not silently throttle successful load. Abort and report overload,
		// rather than letting an unbounded synthetic backlog consume memory.
		if (Submitted - Completed + Shooters * Pellets > 256)
		{
			Stop(TEXT("OutstandingLimit")); return;
		}
		AActor* Anchor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("SnowPerfAnchor")))
			{
				if (Anchor) { Stop(TEXT("AmbiguousAnchor")); return; }
				Anchor = *It;
			}
		}
		AVoxelWorld* FallbackWorld = nullptr;
		for (TActorIterator<AVoxelWorld> It(World); It; ++It)
		{
			if (It->IsCreated())
			{
				if (FallbackWorld) { Stop(TEXT("AmbiguousVoxelWorld")); return; }
				FallbackWorld = *It;
			}
		}
		UDRSnowSubsystem* Snow = World->GetSubsystem<UDRSnowSubsystem>();
		if (!Anchor || !FallbackWorld || !Snow) { Stop(TEXT("MissingAnchorOrVoxelWorld")); return; }
		if (Burst == 0)
		{
			const IConsoleVariable* Fast = IConsoleManager::Get().FindConsoleVariable(TEXT("dr.Snow.FastCompletion"));
			UE_LOG(LogTemp, Log, TEXT("[DRSnowLoad] Stage=Settings PID=%u Run=%s FastCompletion=%d FastReplay=%d VoxelSize=%.3f"),
				FPlatformProcess::GetCurrentProcessId(), *Run, Fast ? Fast->GetInt() : -1,
				CVarDRSnowFastReplay.GetValueOnGameThread(), FallbackWorld->VoxelSize);
		}
		UE_LOG(LogTemp, Log, TEXT("[DRSnowLoad] Stage=Burst PID=%u Run=%s Burst=%d LateMs=%.3f Outstanding=%d"),
			FPlatformProcess::GetCurrentProcessId(), *Run, Burst, (Now - Deadline) * 1000.0, Submitted - Completed);
		for (int32 Shooter = 0; Shooter < Shooters; ++Shooter)
		{
			for (int32 Pellet = 0; Pellet < Pellets; ++Pellet)
			{
				// Six lanes spaced 300 cm; deterministic scatter +/-100 cm.
				const FVector Center = Anchor->GetActorLocation() + FVector(
					(Shooter - (Shooters - 1) * 0.5f) * 300.f + Random.FRandRange(-100.f, 100.f),
					Random.FRandRange(-100.f, 100.f), 0.f);
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(DRSnowLoad), true);
				Params.AddIgnoredActor(Anchor);
				if (!World->LineTraceSingleByChannel(Hit, Center + FVector(0, 0, 10000),
					Center - FVector(0, 0, 10000), ECC_Visibility, Params))
				{
					++Misses; continue;
				}
				AVoxelWorld* Target = Cast<AVoxelWorld>(Hit.GetActor());
				FDRSnowSurfaceAddRequest Request;
				Request.TargetVoxelWorld = Target ? Target : FallbackWorld;
				Request.WorldLocation = Hit.ImpactPoint;
				Request.SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
				Request.ImpactDirection = FVector(0, 0, -1);
				Request.Radius = Radius; Request.Amount = Amount;
				Request.EditTool = EDRSnowVoxelEditTool::DirectionalSurfaceTool;
				Request.bAllowVirtualSurfaceFallback = true;
				Request.bUseVirtualSurface = Target == nullptr;
				Request.Context.TeamId = Shooter % 2;
				FDRSnowAddOperation Operation;
				Operation.WorldLocation = Request.WorldLocation;
				Operation.SurfaceNormal = Request.SurfaceNormal;
				Operation.ImpactDirection = Request.ImpactDirection;
				Operation.Radius = Radius; Operation.Amount = Amount;
				Operation.EditTool = Request.EditTool;
				Operation.bAllowVirtualSurfaceFallback = true;
				Operation.bUseVirtualSurface = Request.bUseVirtualSurface;
				Operation.TeamId = Request.Context.TeamId;
				Operation.VoxelWorldName = Request.TargetVoxelWorld->GetFName();
				const int32 Id = ++Submitted;
				const double SubmittedAt = FPlatformTime::Seconds();
				const auto Self = AsShared();
				Snow->AddSnow(Request, [Self, Operation, Id, SubmittedAt](const float Applied)
				{
					++Self->Completed;
					int32 Sequence = 0;
					if (ADRMiningGameStateBase* Current = Self->State.Get(); IsValid(Current) && Applied > 0.f)
					{
						++Self->Changed;
						Current->RegisterSnowAdd(Operation, Applied);
						Sequence = Current->GetSnowOperationSequence();
					}
					UE_LOG(LogTemp, Log, TEXT("[DRSnowLoad] Stage=Completed PID=%u Run=%s Id=%d Sequence=%d SubmitToRegisterMs=%.3f Applied=%.6f Outstanding=%d"),
						FPlatformProcess::GetCurrentProcessId(), *Self->Run, Id, Sequence,
						(FPlatformTime::Seconds() - SubmittedAt) * 1000.0, Applied, Self->Submitted - Self->Completed);
				});
			}
		}
		++Burst;
		if (Submitted == 0) { Stop(TEXT("NoHits")); return; }
		Schedule(); // One due burst per world tick; LateMs reveals catch-up/overload.
	}
};
#else
struct FDRSnowLoadTest {};
#endif

void ADRMiningGameStateBase::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (HasAuthority())
	{
		FString Spec;
		if (FParse::Value(FCommandLine::Get(), TEXT("SnowLoadTest="), Spec, false))
		{
			TArray<FString> Args;
			Spec.ParseIntoArray(Args, TEXT(","), false);
			StartSnowLoadTest(Args);
		}
	}
#endif
}

void ADRMiningGameStateBase::StartSnowLoadTest(const TArray<FString>& Args)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !GetWorld()->IsGameWorld()) { return; }
	if (SnowLoadTest && (SnowLoadTest->bActive || SnowLoadTest->Submitted != SnowLoadTest->Completed))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DRSnowLoad] Already active or still draining. Restart the map for a clean comparison."));
		return;
	}
	auto Test = MakeShared<FDRSnowLoadTest>();
	if (!Test->Configure(Args))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DRSnowLoad] Usage: dr.Snow.LoadTest.Start [Shooters Pellets IntervalSeconds Bursts Radius Amount WarmupSeconds]. Example: 6 8 1 10 65 1.5 20"));
		return;
	}
	SnowLoadTest = Test;
	Test->State = this;
	Test->Run = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Test->StartTime = FPlatformTime::Seconds();
	Test->bActive = true;
	UE_LOG(LogTemp, Log, TEXT("[DRSnowLoad] Stage=Begin PID=%u Run=%s World=%s Shooters=%d Pellets=%d Interval=%.3f Bursts=%d Radius=%.3f Amount=%.3f Warmup=%.3f Seed=12345"),
		FPlatformProcess::GetCurrentProcessId(), *Test->Run, *GetWorld()->GetName(), Test->Shooters, Test->Pellets,
		Test->Interval, Test->Bursts, Test->Radius, Test->Amount, Test->Warmup);
	Test->Schedule();
#endif
}

void ADRMiningGameStateBase::StopSnowLoadTest()
{
#if !UE_BUILD_SHIPPING
	if (SnowLoadTest) { SnowLoadTest->Stop(TEXT("StoppedOrWorldReset")); }
#endif
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GDRSnowLoadStart(TEXT("dr.Snow.LoadTest.Start"),
	TEXT("Server only: [Shooters Pellets IntervalSeconds Bursts Radius Amount WarmupSeconds]. Needs actor tag SnowPerfAnchor."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (IsValid(World))
		{
			if (auto* State = World->GetGameState<ADRMiningGameStateBase>()) { State->StartSnowLoadTest(Args); }
		}
	}));
static FAutoConsoleCommandWithWorldAndArgs GDRSnowLoadStop(TEXT("dr.Snow.LoadTest.Stop"),
	TEXT("Stop synthetic input; already submitted edits still complete."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (IsValid(World))
		{
			if (auto* State = World->GetGameState<ADRMiningGameStateBase>()) { State->StopSnowLoadTest(); }
		}
	}));
#endif


void ADRMiningGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSnowLoadTest();
	++SnowApplicationGeneration;
	bSnowReplayContinuationScheduled = false;
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
void ADRMiningGameStateBase::RegisterSnowAdd(const FDRSnowAddOperation& Operation)
{
	RegisterSnowAdd(Operation, Operation.Amount);
}

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
	Record.ServerAppliedAmount = ServerAppliedAmount;
	QueueSnowOperationForBroadcast(MoveTemp(Record));
}

void ADRMiningGameStateBase::RegisterSnowRemove(
	const FDRSnowRemoveOperation& Operation)
{
	if (!HasAuthority())
	{
		return;
	}

	FDRSnowOperationRecord Record;
	Record.Sequence = ++NextSnowOperationSequence;
	Record.bIsAddOperation = false;
	Record.RemoveOperation = Operation;
	QueueSnowOperationForBroadcast(MoveTemp(Record));
}

void ADRMiningGameStateBase::QueueSnowOperationForBroadcast(FDRSnowOperationRecord&& Record)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsValid(World))
	{
		return;
	}
	if (!SnowOperationBatcher)
	{
		const TWeakObjectPtr<ADRMiningGameStateBase> WeakThis(this);
		SnowOperationBatcher = MakeShared<FDRSnowOperationBatcher>(*World,
			[WeakThis](const TArray<FDRSnowOperationRecord>& Batch)
			{
				if (ADRMiningGameStateBase* GameState = WeakThis.Get();
					IsValid(GameState) && GameState->HasAuthority())
				{
					for (const FDRSnowOperationRecord& Item : Batch)
					{
						LogSnowReplayPerf(TEXT("ServerBroadcast"), GameState->GetWorld(), Item.Sequence,
							GameState->SnowApplicationGeneration, Batch.Num());
					}
					GameState->Multicast_ApplySnowOperations(Batch);
				}
			});
	}
	LogSnowReplayPerf(TEXT("ServerQueued"), World, Record.Sequence, SnowApplicationGeneration, 0);
	SnowOperationBatcher->Enqueue(MoveTemp(Record));
}

void ADRMiningGameStateBase::ClearSnowOperationBroadcasts()
{
	if (SnowOperationBatcher)
	{
		SnowOperationBatcher->Reset();
	}
}

void ADRMiningGameStateBase::ResetSnowOperationState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Sequence를 0으로 되돌리기 전에 이전 경기의 미전송 배치를 폐기한다.
#if !UE_BUILD_SHIPPING
	// Map/match initialization may reset state during the warmup. No test
	// input exists yet, so keep the armed timer. Reset after input aborts it.
	if (SnowLoadTest && (SnowLoadTest->Submitted > 0 || SnowLoadTest->Burst > 0)) { StopSnowLoadTest(); }
#endif
	ClearSnowOperationBroadcasts();
	NextSnowOperationSequence = 0;
	AppliedSnowCheckpointSequence = 0;
	AppliedSnowOperationSequences.Reset();
	PendingSnowOperations.Reset();
	ActiveDirectionalSnowOperationSequence = INDEX_NONE;
	++SnowApplicationGeneration;
	bSnowReplayContinuationScheduled = false;
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
	ActiveDirectionalSnowOperationSequence = INDEX_NONE;
	++SnowApplicationGeneration;
	bSnowReplayContinuationScheduled = false;
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
	ActiveDirectionalSnowOperationSequence = INDEX_NONE;
	++SnowApplicationGeneration;
	bSnowReplayContinuationScheduled = false;
	StopPendingSnowRetry();
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
		LogSnowReplayPerf(TEXT("Received"), World, Record.Sequence, SnowApplicationGeneration, Records.Num());
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

	// 네트워크 콜백에서는 복셀을 직접 편집하지 않고 공통 Sequence 큐에만 넣는다.
	QueuePendingSnowOperation(Record);
	return false;
}

bool ADRMiningGameStateBase::IsSnowOperationReady(const FDRSnowOperationRecord& Record) const
{
	UWorld* World = GetWorld();
	const UDRSnowSubsystem* SnowSubsystem = IsValid(World)
		? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	if (!IsValid(SnowSubsystem))
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
	if (Record.bIsAddOperation && Record.AddOperation.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		LogSnowReplayPerf(TEXT("Queued"), GetWorld(), Record.Sequence, SnowApplicationGeneration, PendingSnowOperations.Num());
	}
	StartPendingSnowRetry();
}

void ADRMiningGameStateBase::TryApplyPendingSnowOperations()
{
	if (bSnowReplayPumping)
	{
		StartPendingSnowRetry();
		return;
	}
	TGuardValue<bool> PumpGuard(bSnowReplayPumping, true);
	PendingSnowRetryTimer.Invalidate();
	if (ActiveDirectionalSnowOperationSequence != INDEX_NONE)
	{
		return;
	}

	while (!PendingSnowOperations.IsEmpty()
		&& IsSnowOperationApplied(PendingSnowOperations[0].Sequence))
	{
		PendingSnowOperations.RemoveAt(0);
	}

	if (PendingSnowOperations.IsEmpty())
	{
		StopPendingSnowRetry();
		return;
	}

	const FDRSnowOperationRecord Record = PendingSnowOperations[0];
	if (!IsSnowOperationReady(Record))
	{
		StartPendingSnowRetry();
		return;
	}

	const bool bFastReplay = CVarDRSnowFastReplay.GetValueOnGameThread() != 0;
	if (SnowReplayBudgetFrame != GFrameCounter)
	{
		SnowReplayBudgetFrame = GFrameCounter;
		SnowReplayStartsThisFrame = 0;
		SnowReplayDispatchMsThisFrame = 0.0;
	}
	if (bFastReplay && (SnowReplayStartsThisFrame >= FMath::Clamp(CVarDRSnowReplayMaxStarts.GetValueOnGameThread(), 1, 32) ||
		SnowReplayDispatchMsThisFrame >= FMath::Max(0.1f, CVarDRSnowReplayBudgetMs.GetValueOnGameThread())))
	{
		StartPendingSnowRetry();
		return;
	}
	++SnowReplayStartsThisFrame;
	const double DispatchStart = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		SnowReplayDispatchMsThisFrame += (FPlatformTime::Seconds() - DispatchStart) * 1000.0;
	};
	// Keep only one directional operation in flight. A completion may schedule
	// the next sequence in this frame, subject to the start/time budget above.
	bool bApplied = false;
	if (Record.bIsAddOperation)
	{
		const bool bDirectional =
			Record.AddOperation.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool;
		if (bDirectional)
		{
			LogSnowReplayPerf(TEXT("Started"), GetWorld(), Record.Sequence, SnowApplicationGeneration, PendingSnowOperations.Num());
			ActiveDirectionalSnowOperationSequence = Record.Sequence;
		}

		bApplied = ApplySnowAddOnce(Record);
		if (bDirectional)
		{
			if (!bApplied && ActiveDirectionalSnowOperationSequence == Record.Sequence)
			{
				ActiveDirectionalSnowOperationSequence = INDEX_NONE;
				StartPendingSnowRetry();
			}
			return;
		}
	}
	else
	{
		bApplied = ApplySnowRemoveOnce(Record);
	}
	if (!bApplied)
	{
		StartPendingSnowRetry();
		return;
	}
	if (Record.Sequence > 0)
	{
		AppliedSnowOperationSequences.Add(Record.Sequence);
	}
	PendingSnowOperations.RemoveAt(0);
	if (!PendingSnowOperations.IsEmpty())
	{
		StartPendingSnowRetry();
	}
}

void ADRMiningGameStateBase::StartPendingSnowRetry()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetTimerManager().IsTimerActive(PendingSnowRetryTimer))
	{
		return;
	}

	PendingSnowRetryTimer = World->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::TryApplyPendingSnowOperations);
}

void ADRMiningGameStateBase::ScheduleSnowReplayContinuation()
{
	if (CVarDRSnowFastReplay.GetValueOnGameThread() == 0)
	{
		StartPendingSnowRetry();
		return;
	}
	if (bSnowReplayContinuationScheduled)
	{
		return;
	}
	bSnowReplayContinuationScheduled = true;
	const int32 Generation = SnowApplicationGeneration;
	const TWeakObjectPtr<ADRMiningGameStateBase> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation]()
	{
		ADRMiningGameStateBase* State = WeakThis.Get();
		if (!IsValid(State) || State->SnowApplicationGeneration != Generation)
		{
			return;
		}
		State->bSnowReplayContinuationScheduled = false;
		State->StopPendingSnowRetry();
		State->TryApplyPendingSnowOperations();
	});
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
		if (Operation.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
		{
			const int32 OperationSequence = Record.Sequence;
			const int32 ApplicationGeneration = SnowApplicationGeneration;
			const TWeakObjectPtr<ADRMiningGameStateBase> WeakThis(this);
			return SnowSubsystem->ApplyReplicatedSnowAdd(
				Request,
				Record.ServerAppliedAmount,
				[WeakThis, OperationSequence, ApplicationGeneration](const float AppliedAmount)
				{
					if (ADRMiningGameStateBase* GameState = WeakThis.Get())
					{
						GameState->HandleDirectionalSnowAddCompleted(
							OperationSequence,
							ApplicationGeneration,
							AppliedAmount);
					}
				}).AddedAmount > 0.f;
		}

		SnowSubsystem->ApplyReplicatedSnowAdd(Request, Record.ServerAppliedAmount);
		// 이미 채워진 눈벽처럼 로컬 변화가 없어도 재시도하며 큐를 막지 않는다.
		return true;
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
	return SnowSubsystem->ApplyReplicatedSnowRemoval(
		Request,
		Operation.AppliedAmount);
}

void ADRMiningGameStateBase::HandleDirectionalSnowAddCompleted(
	const int32 OperationSequence,
	const int32 ApplicationGeneration,
	const float AppliedAmount)
{
	if (ApplicationGeneration != SnowApplicationGeneration ||
		ActiveDirectionalSnowOperationSequence != OperationSequence)
	{
		return;
	}

	LogSnowReplayPerf(TEXT("Completed"), GetWorld(), OperationSequence, ApplicationGeneration, PendingSnowOperations.Num());
	ActiveDirectionalSnowOperationSequence = INDEX_NONE;
	if (OperationSequence > 0)
	{
		AppliedSnowOperationSequences.Add(OperationSequence);
	}
	PendingSnowOperations.RemoveAll(
		[OperationSequence](const FDRSnowOperationRecord& Record)
		{
			return Record.Sequence == OperationSequence;
		});

	if (AppliedAmount <= 0.f)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[SnowReplication] Directional add completed without a local voxel change. Sequence=%d"),
			OperationSequence);
	}

	if (!PendingSnowOperations.IsEmpty())
	{
		ScheduleSnowReplayContinuation();
	}
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
