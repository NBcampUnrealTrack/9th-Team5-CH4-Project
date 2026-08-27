#include "DRVoxelTerrainAreaSyncActor.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif
#include "Engine/World.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	// 관리 영역과 검사 영역을 같은 규칙으로 그립니다.
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

ADRVoxelTerrainAreaSyncActor::ADRVoxelTerrainAreaSyncActor()
{
	// Tick은 에디터 영역 표시에서만 사용합니다.
#if WITH_EDITOR
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif

	// 모든 클라이언트에 반복 Multicast를 보내도록 채널을 유지합니다.
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
}

#if WITH_EDITOR
void ADRVoxelTerrainAreaSyncActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld())
	{
		return;
	}

	// 회전 없는 계산 영역을 매 프레임 다시 그립니다.
	DrawDepositAreaBox(
		World,
		GetActorLocation(),
		BoxExtent,
		FColor(64, 200, 255),
		false,
		0.f,
		2.f);
}

bool ADRVoxelTerrainAreaSyncActor::ShouldTickIfViewportsOnly() const
{
	// 게임 전 에디터 뷰포트에서도 영역 표시 Tick을 허용합니다.
	return true;
}
#endif

bool ADRVoxelTerrainAreaSyncActor::MakeDepositCommand(
	FDRVoxelDepositCommand& OutCommand) const
{
	// 실패 시 이전 결과가 남지 않도록 출력을 초기화합니다.
	OutCommand = FDRVoxelDepositCommand();
	// RPC 전파 전에 월드와 설정 값을 검증합니다.
	if (!IsValid(GetWorld()) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		GetActorLocation().ContainsNaN() || BoxExtent.ContainsNaN() ||
		!FMath::IsFinite(RandomScanWorldSize) || RandomScanWorldSize <= 0.f)
	{
		return false;
	}

	// Extent는 절댓값으로 처리하고 빈 축은 거부합니다.
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
	// 명령 시드로 모든 인스턴스의 결과를 일치시킵니다.
	OutCommand.Settings.RandomSeed = FMath::Rand();
	// 검사 영역은 X/Y만 줄이고 Z는 관리 영역 전체를 사용합니다.
	const float RequestedHalfSize = RandomScanWorldSize * 0.5f;
	OutCommand.ScanExtent = FVector(
		FMath::Min(AreaExtent.X, RequestedHalfSize),
		FMath::Min(AreaExtent.Y, RequestedHalfSize),
		AreaExtent.Z);
	// 검사 위치와 표본 선택에 서로 다른 결정적 난수 흐름을 사용합니다.
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

void ADRVoxelTerrainAreaSyncActor::BeginPlay()
{
	Super::BeginPlay();

#if ENABLE_DRAW_DEBUG
	// 게임에서는 관리 영역을 영구 디버그 박스로 한 번 그립니다.
	DrawDepositAreaBox(
		GetWorld(),
		GetActorLocation(),
		BoxExtent,
		FColor(64, 200, 255),
		true,
		-1.f,
		2.f);
#endif

	// 런타임에는 디버그 표시용 Tick을 끕니다.
	SetActorTickEnabled(false);

	// 클라이언트는 RPC를 받을 때만 퇴적을 처리합니다.
	if (!HasAuthority())
	{
		return;
	}

	// 비활성 상태에서도 타이머를 유지하며 즉시 첫 주기를 시작합니다.
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

void ADRVoxelTerrainAreaSyncActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 파괴 전에 예약 작업과 로컬 상태를 정리합니다.
	CancelDepositPipeline();
	Super::EndPlay(EndPlayReason);
}

void ADRVoxelTerrainAreaSyncActor::RequestDepositArea()
{
	// 서버만 새 퇴적 명령을 만듭니다.
	if (!HasAuthority())
	{
		return;
	}

	// 비활성화 시 진행 중인 작업은 유지하고 새 요청만 막습니다.
	if (!bEnableDepositAccumulation)
	{
		return;
	}
	// 이전 명령이 끝날 때까지 새 RPC 생성을 막습니다.
	if (!PreparedDepositPlan.IsEmpty() || QueuedDepositCommands.Num() > 0 ||
		DepositPipelineTimerHandle.IsValid())
	{
		return;
	}

	// VoxelWorld가 준비되지 않으면 다음 주기에 다시 시도합니다.
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

	// Multicast는 서버와 모든 클라이언트에서 같은 처리 경로를 실행합니다.
	MulticastPrepareDeposit(Command);
}

void ADRVoxelTerrainAreaSyncActor::MulticastPrepareDeposit_Implementation(
	const FDRVoxelDepositCommand& Command)
{
#if ENABLE_DRAW_DEBUG
	// 수신한 검사 영역을 1초간 표시합니다.
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

void ADRVoxelTerrainAreaSyncActor::PrepareNextQueuedDeposit()
{
	// 실행된 다음 틱 타이머 핸들을 직접 무효화합니다.
	DepositPipelineTimerHandle.Invalidate();
	if (!PreparedDepositPlan.IsEmpty() || QueuedDepositCommands.Num() == 0)
	{
		return;
	}

	// 실패한 명령을 재시도하지 않도록 대기열에서 먼저 제거합니다.
	const FDRVoxelDepositCommand Command = QueuedDepositCommands[0];
	QueuedDepositCommands.RemoveAt(0, 1, EAllowShrinking::No);

	// 준비 단계는 데이터를 읽기만 합니다.
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

	// 다음 명령도 다음 프레임부터 순서대로 처리합니다.
	if (QueuedDepositCommands.Num() > 0)
	{
		DepositPipelineTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::PrepareNextQueuedDeposit);
	}
}

void ADRVoxelTerrainAreaSyncActor::ApplyPreparedDeposit()
{
	// 새 작업을 예약할 수 있도록 실행된 핸들을 비웁니다.
	DepositPipelineTimerHandle.Invalidate();
	if (PreparedDepositPlan.IsEmpty())
	{
		return;
	}

	// 적용 결과와 관계없이 계획은 비워집니다.
	if (!FDRVoxelDepositOperations::ApplyDepositPlan(
		VoxelWorld,
		PreparedDepositPlan))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to apply prepared deposit."));
	}

	// 다음 준비를 다음 프레임으로 미룹니다.
	if (QueuedDepositCommands.Num() > 0)
	{
		DepositPipelineTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::PrepareNextQueuedDeposit);
	}
}

void ADRVoxelTerrainAreaSyncActor::CancelDepositPipeline()
{
	// 다음 틱 작업과 준비된 로컬 상태만 명시적으로 정리합니다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DepositPipelineTimerHandle);
	}
	DepositPipelineTimerHandle.Invalidate();
	PreparedDepositPlan.Reset();
	QueuedDepositCommands.Reset();
}
