#include "DRVoxelDepositArea.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	void DrawDepositAreaBox(
		UWorld* World,
		const FVector& Center,
		const FVector& Extent,
		const FColor& Color,
		bool bPersistentLines,
		float LifeTime,
		float Thickness)
	{
		if (!IsValid(World))
		{
			return;
		}

		DrawDebugBox(
			World,
			Center,
			FVector(FMath::Abs(Extent.X), FMath::Abs(Extent.Y), FMath::Abs(Extent.Z)),
			Color,
			bPersistentLines,
			LifeTime,
			0,
			Thickness);
	}
}
#endif

ADRVoxelDepositArea::ADRVoxelDepositArea()
{
#if WITH_EDITOR
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif

	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
}

#if WITH_EDITOR
void ADRVoxelDepositArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld())
	{
		return;
	}

	DrawDepositAreaBox(
		World,
		GetActorLocation(),
		BoxExtent,
		FColor(64, 200, 255),
		false,
		0.f,
		2.f);
}

bool ADRVoxelDepositArea::ShouldTickIfViewportsOnly() const
{
	return true;
}
#endif

bool ADRVoxelDepositArea::MakeDepositCommand(
	FDRVoxelDepositCommand& OutCommand) const
{
	// 실패 시 이전 결과가 남지 않도록 출력을 초기화합니다.
	OutCommand = FDRVoxelDepositCommand();
	if (!IsValid(GetWorld()) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		GetActorLocation().ContainsNaN() || BoxExtent.ContainsNaN() ||
		!FMath::IsFinite(RandomScanWorldSize) || RandomScanWorldSize <= 0.f)
	{
		return false;
	}

	const FVector AreaExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));
	if (AreaExtent.X <= KINDA_SMALL_NUMBER ||
		AreaExtent.Y <= KINDA_SMALL_NUMBER ||
		AreaExtent.Z <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCommand.Settings = DepositSettings;
	OutCommand.Settings.RandomSeed = FMath::Rand();
	// 검사 영역은 X/Y만 줄이고 Z는 관리 영역 전체를 사용합니다.
	const float RequestedHalfSize = RandomScanWorldSize * 0.5f;
	OutCommand.ScanExtent = FVector(
		FMath::Min(AreaExtent.X, RequestedHalfSize),
		FMath::Min(AreaExtent.Y, RequestedHalfSize),
		AreaExtent.Z);
	FRandomStream ScanAreaRandomStream(
		OutCommand.Settings.RandomSeed ^ 0x27D4EB2D);
	// 검사 영역이 관리 영역 안에 있도록 중심 이동 범위를 제한합니다.
	OutCommand.ScanCenter = GetActorLocation() + FVector(
		ScanAreaRandomStream.FRandRange(
			-(AreaExtent.X - OutCommand.ScanExtent.X),
			AreaExtent.X - OutCommand.ScanExtent.X),
		ScanAreaRandomStream.FRandRange(
			-(AreaExtent.Y - OutCommand.ScanExtent.Y),
			AreaExtent.Y - OutCommand.ScanExtent.Y),
		0.f);
	OutCommand.AreaCenter = GetActorLocation();
	OutCommand.AreaExtent = AreaExtent;
	OutCommand.RequiredStaticMeshSurfaceTag = RequiredStaticMeshSurfaceTag;
	OutCommand.MaxStaticMeshSlopeAngle = MaxStaticMeshSlopeAngle;
	OutCommand.bDepositOnStaticMeshes = bDepositOnStaticMeshes;
	OutCommand.bTraceComplexStaticMeshSurfaces = bTraceComplexStaticMeshSurfaces;
	return true;
}

void ADRVoxelDepositArea::BeginPlay()
{
	Super::BeginPlay();

#if ENABLE_DRAW_DEBUG
	DrawDepositAreaBox(
		GetWorld(),
		GetActorLocation(),
		BoxExtent,
		FColor(64, 200, 255),
		true,
		-1.f,
		2.f);
#endif

	SetActorTickEnabled(false);

	if (!HasAuthority())
	{
		return;
	}

	if (ADRMiningGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>())
	{
		GameMode->OnJoinSnapshotStarted.AddUObject(
			this,
			&ThisClass::HandleJoinSnapshotStarted);
		GameMode->OnJoinSnapshotFinished.AddUObject(
			this,
			&ThisClass::HandleJoinSnapshotFinished);
	}

	if (DepositInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DepositTimerHandle,
			this,
			&ThisClass::RequestDepositArea,
			DepositInterval,
			true,
			0.f);
	}
}

void ADRVoxelDepositArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	ADRMiningGameModeBase* GameMode = IsValid(World)
		? World->GetAuthGameMode<ADRMiningGameModeBase>()
		: nullptr;
	if (IsValid(GameMode))
	{
		GameMode->OnJoinSnapshotStarted.RemoveAll(this);
		GameMode->OnJoinSnapshotFinished.RemoveAll(this);
	}

	CancelDepositPipeline();
	Super::EndPlay(EndPlayReason);
}

void ADRVoxelDepositArea::HandleJoinSnapshotStarted()
{
	if (!HasAuthority())
	{
		return;
	}

	++ActiveJoinSnapshotCount;
}

void ADRVoxelDepositArea::HandleJoinSnapshotFinished(EDRSnowJoinSnapshotResult)
{
	if (!HasAuthority())
	{
		return;
	}

	ActiveJoinSnapshotCount = FMath::Max(0, ActiveJoinSnapshotCount - 1);
}

void ADRVoxelDepositArea::RequestDepositArea()
{
	if (!HasAuthority() || ActiveJoinSnapshotCount > 0)
	{
		return;
	}

	// 이전 명령이 끝날 때까지 새 RPC 생성을 막습니다.
	if (!PreparedDepositPlan.IsEmpty() || QueuedDepositCommands.Num() > 0 ||
		DepositPipelineTimerHandle.IsValid())
	{
		return;
	}

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
	}

	FDRVoxelDepositCommand Command;
	if (!MakeDepositCommand(Command))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Failed to create random surface deposit command."));
		return;
	}

	MulticastPrepareDeposit(Command);
}

void ADRVoxelDepositArea::MulticastPrepareDeposit_Implementation(
	const FDRVoxelDepositCommand& Command)
{
#if ENABLE_DRAW_DEBUG
	DrawDepositAreaBox(
		GetWorld(),
		Command.ScanCenter,
		Command.ScanExtent,
		FColor(255, 165, 0),
		false,
		1.f,
		3.f);
#endif

	// 명령을 수신 순서대로 대기열에 추가합니다.
	QueuedDepositCommands.Add(Command);
	// 파이프라인이 비어 있을 때만 즉시 준비를 시작합니다.
	if (PreparedDepositPlan.IsEmpty() && !DepositPipelineTimerHandle.IsValid())
	{
		PrepareNextQueuedDeposit();
	}
}

void ADRVoxelDepositArea::PrepareNextQueuedDeposit()
{
	DepositPipelineTimerHandle.Invalidate();
	if (!PreparedDepositPlan.IsEmpty() || QueuedDepositCommands.Num() == 0)
	{
		return;
	}

	// 실패한 명령을 재시도하지 않도록 대기열에서 먼저 제거합니다.
	const FDRVoxelDepositCommand Command = QueuedDepositCommands[0];
	QueuedDepositCommands.RemoveAt(0, 1, EAllowShrinking::No);

	if (!FDRVoxelDepositOperations::PrepareDepositCommand(
		GetWorld(),
		VoxelWorld,
		this,
		Command,
		PreparedDepositPlan))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to prepare deposit RPC command."));
	}
	else if (!PreparedDepositPlan.IsEmpty())
	{
		// 준비와 적용을 서로 다른 프레임에 실행합니다.
		DepositPipelineTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::ApplyPreparedDeposit);
		return;
	}

	if (QueuedDepositCommands.Num() > 0)
	{
		DepositPipelineTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::PrepareNextQueuedDeposit);
	}
}

void ADRVoxelDepositArea::ApplyPreparedDeposit()
{
	DepositPipelineTimerHandle.Invalidate();
	if (PreparedDepositPlan.IsEmpty())
	{
		return;
	}

	if (!FDRVoxelDepositOperations::ApplyDepositPlan(
		VoxelWorld,
		PreparedDepositPlan))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to apply prepared deposit."));
	}

	if (QueuedDepositCommands.Num() > 0)
	{
		DepositPipelineTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::PrepareNextQueuedDeposit);
	}
}

void ADRVoxelDepositArea::CancelDepositPipeline()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DepositPipelineTimerHandle);
	}
	DepositPipelineTimerHandle.Invalidate();
	PreparedDepositPlan.Reset();
	QueuedDepositCommands.Reset();
}
