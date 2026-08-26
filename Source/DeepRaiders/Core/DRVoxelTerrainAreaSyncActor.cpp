#include "DRVoxelTerrainAreaSyncActor.h"

#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

namespace
{
	constexpr float DRDepositBatchIntervalSeconds = 0.1f;
}

ADRVoxelTerrainAreaSyncActor::ADRVoxelTerrainAreaSyncActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// 이 액터가 직접 Revision과 델타 배열을 복제한다. 중도 난입 클라이언트도 전체 기록을 받아야 하므로
	// 항상 관련 액터로 유지하고, 휴면 상태로 들어가 새 델타 복제가 멈추지 않게 한다.
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
	TerrainEdits.SetOwner(this);
}

ADRVoxelTerrainAreaSyncActor::~ADRVoxelTerrainAreaSyncActor() = default;

void ADRVoxelTerrainAreaSyncActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 에디터에서 액터 이동, BoxExtent 변경, 청크 크기 변경 결과를 즉시 볼 수 있게 한다.
	RebuildTerrainChunks();
}

#if WITH_EDITOR
bool ADRVoxelTerrainAreaSyncActor::ShouldTickIfViewportsOnly() const
{
	return true;
}
#endif

void ADRVoxelTerrainAreaSyncActor::RebuildTerrainChunks()
{
	// 레이아웃이 바뀌는 동안 이전 청크 요청을 계속 처리하면 서로 다른 경계의 결과가 한 패스에 섞인다.
	// 런타임 변경도 안전하게 반영할 수 있도록 진행 상태를 먼저 취소한 뒤 새 XY 청크 배열을 만든다.
	CancelDepositPass();
	TerrainChunks.Reset();
	bHasCachedChunkLayout = true;
	CachedChunkCenter = GetActorLocation();
	CachedChunkExtent = BoxExtent;
	CachedDepositChunkWorldSize = DepositChunkWorldSize;

	if (!FMath::IsFinite(DepositChunkWorldSize) || DepositChunkWorldSize <= 0.f ||
		CachedChunkCenter.ContainsNaN() || CachedChunkExtent.ContainsNaN())
	{
		return;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.X <= KINDA_SMALL_NUMBER ||
		AbsExtent.Y <= KINDA_SMALL_NUMBER ||
		AbsExtent.Z <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const FVector RegionMin = Center - AbsExtent;
	const FVector RegionMax = Center + AbsExtent;
	const int64 ChunkCountX = FMath::CeilToInt64(
		static_cast<double>(AbsExtent.X) * 2.0 / DepositChunkWorldSize);
	const int64 ChunkCountY = FMath::CeilToInt64(
		static_cast<double>(AbsExtent.Y) * 2.0 / DepositChunkWorldSize);
	if (ChunkCountX <= 0 || ChunkCountY <= 0 ||
		ChunkCountX > MAX_int32 || ChunkCountY > MAX_int32 ||
		ChunkCountX > MAX_int32 / ChunkCountY)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Terrain chunk count exceeds the supported range."));
		return;
	}
	const int32 TotalChunkCount = static_cast<int32>(ChunkCountX * ChunkCountY);

	TerrainChunks.Reserve(TotalChunkCount);

	// 각 청크는 월드 XY 평면에서만 나뉘며 Z 중심과 반크기는 전체 관리 박스와 같다.
	// 가장자리 청크는 영역 끝에서 잘라 실제 BoxCenter/BoxExtent를 저장하므로 별도 복셀 경계 변환이 필요 없다.
	for (int32 ChunkX = 0; ChunkX < static_cast<int32>(ChunkCountX); ++ChunkX)
	{
		const float ChunkMinX = RegionMin.X + ChunkX * DepositChunkWorldSize;
		const float ChunkMaxX = FMath::Min(ChunkMinX + DepositChunkWorldSize, RegionMax.X);

		for (int32 ChunkY = 0; ChunkY < static_cast<int32>(ChunkCountY); ++ChunkY)
		{
			const float ChunkMinY = RegionMin.Y + ChunkY * DepositChunkWorldSize;
			const float ChunkMaxY = FMath::Min(ChunkMinY + DepositChunkWorldSize, RegionMax.Y);

			FDRVoxelTerrainChunkBounds& Chunk = TerrainChunks.AddDefaulted_GetRef();
			Chunk.ChunkCoordinate = FIntPoint(ChunkX, ChunkY);
			Chunk.BoxCenter = FVector(
				(ChunkMinX + ChunkMaxX) * 0.5f,
				(ChunkMinY + ChunkMaxY) * 0.5f,
				Center.Z);
			Chunk.BoxExtent = FVector(
				(ChunkMaxX - ChunkMinX) * 0.5f,
				(ChunkMaxY - ChunkMinY) * 0.5f,
				AbsExtent.Z);
		}
	}
}

void ADRVoxelTerrainAreaSyncActor::EnsureTerrainChunksCurrent()
{
	// 청크는 월드 공간 XY 경계만 저장하므로 VoxelWorld의 변환이나 VoxelSize는 레이아웃 입력이 아니다.
	// 액터 박스나 월드 청크 크기가 바뀐 경우에만 재계산한다.
	const bool bLayoutIsCurrent =
		bHasCachedChunkLayout &&
		CachedChunkCenter.Equals(GetActorLocation()) &&
		CachedChunkExtent.Equals(BoxExtent) &&
		FMath::IsNearlyEqual(CachedDepositChunkWorldSize, DepositChunkWorldSize);

	if (!bLayoutIsCurrent)
	{
		RebuildTerrainChunks();
	}
}

FDRVoxelTerrainOperationContext ADRVoxelTerrainAreaSyncActor::MakeTerrainOperationContext()
{
	FDRVoxelTerrainOperationContext Context;
	Context.World = GetWorld();
	Context.VoxelWorld = VoxelWorld;
	Context.TraceOwner = this;
	Context.TerrainChunks = &TerrainChunks;
	Context.DepositSettings = &DepositSettings;
	Context.AreaCenter = GetActorLocation();
	Context.AreaExtent = BoxExtent;
	Context.PerformancePreset = PerformancePreset;
	Context.RequiredStaticMeshSurfaceTag = RequiredStaticMeshSurfaceTag;
	Context.MaxStaticMeshSlopeAngle = MaxStaticMeshSlopeAngle;
	Context.bHasAuthority = HasAuthority();
	Context.bEnableDepositAccumulation = bEnableDepositAccumulation;
	Context.bDepositOnStaticMeshes = bDepositOnStaticMeshes;
	Context.bBlockDepositBelowStaticMeshes = bBlockDepositBelowStaticMeshes;
	Context.bTraceComplexStaticMeshSurfaces = bTraceComplexStaticMeshSurfaces;
	return Context;
}

void ADRVoxelTerrainAreaSyncActor::BeginPlay()
{
	Super::BeginPlay();
	// PIE 복제나 객체 복사 경로에서도 런타임 콜백 대상이 반드시 현재 액터를 가리키게 한다.
	TerrainEdits.SetOwner(this);
	EnsureTerrainChunksCurrent();

	// 클라이언트는 타이머로 스캔하거나 퇴적을 생성하지 않고 복제된 델타만 재생한다.
	if (!HasAuthority())
	{
		return;
	}

	// 서버는 자신의 월드에 편집을 직접 적용하므로 기존 기록을 재생하지 않는다.
	// 현재 Revision을 적용 완료 지점으로 맞춘 뒤 이후 편집만 새 기록으로 생성한다.
	TerrainSyncClientState.LastAppliedRevision = TerrainEditRevision;
	BindTerrainDugDelegate();

	// 누적 기능이 현재 꺼져 있어도 타이머는 유지한다. 런타임에 기능을 켜면 다음 주기부터 요청을 만들 수 있다.
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
	CancelDepositPass();
	UnbindTerrainDugDelegate();
	Super::EndPlay(EndPlayReason);
}

void ADRVoxelTerrainAreaSyncActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsureTerrainChunksCurrent();

	DrawTerrainChunkBoxes();

	// 서버와 클라이언트가 같은 복셀 편집을 동시에 계산하지 않도록 역할을 분리한다.
	// 서버는 원본 데이터를 변경하고 델타를 만들며, 클라이언트는 복제된 결과만 적용한다.
	if (HasAuthority())
	{
		// 시간 기반 flush를 먼저 처리해 이전 Tick까지 누적된 변경이 배치 주기를 넘기지 않게 한다.
		// Job이 아래에서 만든 새 델타는 이후 0.1초 배치 또는 청크 완료 경계에서 확정된다.
		if (FDRVoxelTerrainSyncLibrary::AdvanceDepositBatchTimer(
			TerrainSyncServerState,
			DeltaSeconds,
			DRDepositBatchIntervalSeconds))
		{
			FlushPendingDepositBatch();
		}

		if (!bEnableDepositAccumulation &&
			UDRVoxelTerrainOperationLibrary::IsDepositPassActive(TerrainOperationState))
		{
			CancelDepositPass();
		}

		// 지형 상태를 Actor가 직접 검사하지 않는다. Tick 결과의 델타와 경계 플래그만 소비해
		// 퇴적 계산 상태 머신과 네트워크 Revision 생명주기를 분리한다.
		const FDRVoxelTerrainOperationTickResult OperationResult =
			UDRVoxelTerrainOperationLibrary::TickDeposit(
				MakeTerrainOperationContext(),
				TerrainOperationState);
		if (OperationResult.ModifiedVoxelCount > 0 &&
			OperationResult.DeltaRecord.Deltas.Num() > 0)
		{
			FDRVoxelTerrainSyncLibrary::AccumulateDepositDelta(
				OperationResult.DeltaRecord,
				TerrainSyncServerState);
		}
		if (OperationResult.bChunkCompleted || OperationResult.bCancelled)
		{
			// 청크 사이에서 배치를 끊어 한 지형 작업 청크의 완료 시점을 네트워크에도 즉시 반영한다.
			// 취소 전까지 월드에 이미 적용된 값 역시 유실하지 않고 Revision으로 확정한다.
			FlushPendingDepositBatch();
		}

		if (OperationResult.bProcessedRequest)
		{
			UE_LOG(
				LogTemp,
				Verbose,
				TEXT("Deposit processed. Revision=%d ModifiedVoxelCount=%d ScannedColumnCount=%d RemainingRequestCount=%d PendingVoxelCount=%d"),
				TerrainEditRevision,
				OperationResult.ModifiedVoxelCount,
				OperationResult.ScannedColumnCount,
				OperationResult.RemainingRequestCount,
				TerrainSyncServerState.PendingDepositVoxelValues.Num());
		}
	}
	else
	{
		// 클라이언트는 퇴적 계산을 실행하지 않는다. 복제 프로퍼티와 FastArray 중 어느 쪽이
		// 먼저 도착해도 HasPendingEdits가 재생 필요성을 감지하고 연속 Revision만 적용한다.
		if (FDRVoxelTerrainSyncLibrary::HasPendingEdits(
			TerrainEdits.Items,
			TerrainEditRevision,
			TerrainSyncClientState))
		{
			ApplyPendingDeltaRecords();
		}
	}
}

void ADRVoxelTerrainAreaSyncActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, VoxelWorld);
	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, TerrainEditRevision);
	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, TerrainEdits);
}

void ADRVoxelTerrainAreaSyncActor::OnRep_VoxelWorld()
{
	RebuildTerrainChunks();
	ApplyPendingDeltaRecords();
}

void ADRVoxelTerrainAreaSyncActor::OnRep_TerrainDeltaState()
{
	// 최종 Revision이 FastArray 항목보다 먼저 도착해도 누락 Revision 검사로 뒤 항목 적용을 보류한다.
	ApplyPendingDeltaRecords();
}

void ADRVoxelTerrainAreaSyncActor::HandleReplicatedTerrainEdits()
{
	ApplyPendingDeltaRecords();
}

void ADRVoxelTerrainAreaSyncActor::RequestDepositArea()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bEnableDepositAccumulation)
	{
		// 기능을 끄는 즉시 이전 표면을 기준으로 만들어진 진행 중 패스 전체를 폐기한다.
		CancelDepositPass();
		return;
	}

	if (UDRVoxelTerrainOperationLibrary::IsDepositPassActive(TerrainOperationState))
	{
		// 타이머 주기보다 전체 청크 패스가 오래 걸려도 패스를 중첩하지 않는다.
		// 진행 중 패스가 모든 청크를 끝낸 다음 타이머 호출에서만 새 패스를 시작한다.
		return;
	}

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		CancelDepositPass();
		return;
	}

	EnsureTerrainChunksCurrent();
	if (TerrainChunks.Num() == 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("No valid deposit chunks were generated."));
		return;
	}

	// 지형 라이브러리가 현재 청크 목록의 스냅샷 순서를 만들고 첫 단일 요청까지 준비한다.
	// 이후 모든 진행은 Tick 예산으로 나뉘므로 타이머 콜백에서 긴 작업을 수행하지 않는다.
	UDRVoxelTerrainOperationLibrary::StartDepositPass(
		MakeTerrainOperationContext(),
		TerrainOperationState);
}

void ADRVoxelTerrainAreaSyncActor::CancelDepositPass()
{
	// 이미 서버 지형에 반영된 대기 값은 취소와 함께 버리지 않고 먼저 복제 기록으로 확정한다.
	FlushPendingDepositBatch();
	UDRVoxelTerrainOperationLibrary::CancelDeposit(TerrainOperationState);
}

void ADRVoxelTerrainAreaSyncActor::FlushPendingDepositBatch()
{
	if (!HasAuthority())
	{
		return;
	}

	// SyncLibrary는 Revision을 부여하지 않는다. 순수하게 대기 복셀을 wire-format 항목으로 변환하고,
	// Actor가 아래 루프의 실제 추가 순서대로 하나의 전역 Revision 스트림을 만든다.
	TArray<FDRTerrainEditFastArrayItem> Items;
	const int32 BatchedVoxelCount =
		FDRVoxelTerrainSyncLibrary::ConsumeDepositBatch(TerrainSyncServerState, Items);
	if (Items.Num() == 0)
	{
		return;
	}

	for (FDRTerrainEditFastArrayItem& Item : Items)
	{
		AddTerrainEditItem(MoveTemp(Item));
	}

	// 모든 항목을 Dirty 처리한 뒤 오래된 prefix를 정리하고 즉시 네트워크 업데이트를 요청한다.
	TrimReplicatedTerrainRecords();
	ForceNetUpdate();

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Deposit batch replicated. FinalRevision=%d VoxelCount=%d ChunkRecordCount=%d"),
		TerrainEditRevision,
		BatchedVoxelCount,
		Items.Num());
}

void ADRVoxelTerrainAreaSyncActor::AddTerrainEditItem(
	FDRTerrainEditFastArrayItem&& Item)
{
	check(HasAuthority());
	// 퇴적과 굴착이 같은 카운터를 사용해야 클라이언트에서 실제 서버 편집 순서를 그대로 재생할 수 있다.
	Item.Revision = ++TerrainEditRevision;
	FDRTerrainEditFastArrayItem& AddedItem =
		TerrainEdits.Items.Add_GetRef(MoveTemp(Item));
	TerrainEdits.MarkItemDirty(AddedItem);
}


void ADRVoxelTerrainAreaSyncActor::ApplyPendingDeltaRecords()
{
	if (HasAuthority())
	{
		return;
	}

	// 적용과 커서 전진은 SyncLibrary가 원자적으로 관리한다. Actor는 결과에 따라 로그만 남기며,
	// 동일 누락에 대한 반복 로그 여부도 Result.bShouldReport 정책을 따른다.
	const FDRTerrainSyncReplayResult Result =
		FDRVoxelTerrainSyncLibrary::ReplayAvailableEdits(
			VoxelWorld,
			TerrainEdits.Items,
			TerrainEditRevision,
			TerrainSyncClientState);
	if (!Result.bShouldReport)
	{
		return;
	}

	if (Result.Issue == EDRTerrainSyncReplayIssue::RevisionGap)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Terrain delta revision gap. Expected=%d Received=%d. Later deltas will wait."),
			Result.ExpectedRevision,
			Result.AvailableRevision);
	}
	else if (Result.Issue == EDRTerrainSyncReplayIssue::RecordUnavailable)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Terrain delta unavailable. Expected=%d ServerRevision=%d."),
			Result.ExpectedRevision,
			Result.AvailableRevision);
	}
}

void ADRVoxelTerrainAreaSyncActor::TrimReplicatedTerrainRecords()
{
	// 기록을 제거한 뒤 접속한 클라이언트는 누락분을 복원할 수 없으므로 체크포인트가 있을 때만 제한값을 사용한다.
	const int32 RecordsToRemove = FDRVoxelTerrainSyncLibrary::CalculateTrimCount(
		TerrainEdits.Items,
		TerrainEditRevision,
		MaxReplicatedDeltaRecords);
	if (RecordsToRemove > 0)
	{
		TerrainEdits.Items.RemoveAt(0, RecordsToRemove, EAllowShrinking::No);
		TerrainEdits.MarkArrayDirty();
	}
}

void ADRVoxelTerrainAreaSyncActor::BindTerrainDugDelegate()
{
	// 굴착을 실제로 수행하는 기존 서브시스템 코드를 수정하지 않고 완료 이벤트만 구독한다.
	// 서버만 기록을 생성하며 Handle 유효성 검사로 중복 바인딩을 막는다.
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsValid(World) || TerrainDugDelegateHandle.IsValid())
	{
		return;
	}

	UDRVoxelTerrainSubsystem* Subsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(Subsystem))
	{
		return;
	}

	TerrainSubsystem = Subsystem;
	TerrainDugDelegateHandle = Subsystem->OnTerrainDug.AddUObject(
		this,
		&ThisClass::HandleTerrainDug);
}

void ADRVoxelTerrainAreaSyncActor::UnbindTerrainDugDelegate()
{
	// 서브시스템이 먼저 파괴된 경우를 고려해 WeakObjectPtr가 아직 유효할 때만 제거한다.
	if (UDRVoxelTerrainSubsystem* Subsystem = TerrainSubsystem.Get())
	{
		Subsystem->OnTerrainDug.Remove(TerrainDugDelegateHandle);
	}

	TerrainDugDelegateHandle.Reset();
	TerrainSubsystem.Reset();
}

void ADRVoxelTerrainAreaSyncActor::HandleTerrainDug(const FVector& Location, float Radius)
{
	if (!HasAuthority() || Radius <= 0.f)
	{
		return;
	}

	// 맵 전체 굴착 이벤트 중 이 액터의 관리 박스와 구가 겹치는 경우만 동기화 기록에 포함한다.
	if (!UDRVoxelTerrainOperationLibrary::IsVoxelUpdateInBox(
		GetActorLocation(),
		BoxExtent,
		Location,
		Radius))
	{
		return;
	}

	// 진행 중인 요청의 후보 표면은 굴착 이전 높이를 기준으로 계산됐을 수 있다.
	// 그대로 쓰면 파낸 공간 위에 오래된 후보가 쌓일 수 있으므로 현재 청크뿐 아니라 남은 패스 순서와
	// 비동기 메시 트레이스까지 함께 폐기한다. 이때 이미 적용된 퇴적은 먼저 Revision으로 확정한다.
	CancelDepositPass();

	// 이 델리게이트는 서버의 실제 굴착이 끝난 뒤 호출된다. 여기서 서버 지형을 다시 수정하지 않고
	// 클라이언트 재생에 필요한 위치, 반지름, Revision만 기록해 중복 굴착을 방지한다.
	FDRTerrainEditFastArrayItem DigItem;
	DigItem.Type = EDRTerrainEditType::Dig;
	DigItem.Location = Location;
	DigItem.Radius = Radius;
	AddTerrainEditItem(MoveTemp(DigItem));

	TrimReplicatedTerrainRecords();
	ForceNetUpdate();
}

void ADRVoxelTerrainAreaSyncActor::DrawTerrainChunkBoxes() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || MaxDebugTerrainChunkBoxes <= 0)
	{
		return;
	}

	// 청크는 처음부터 월드 공간 XY 박스로 저장된다. 모든 청크가 전체 Z 반크기를 공유하므로
	// 디버그 박스 하나가 실제 표면 스캔 요청 범위와 정확히 일치한다.
	const int32 ChunkBoxesToDraw = FMath::Min(TerrainChunks.Num(), MaxDebugTerrainChunkBoxes);

	for (int32 ChunkIndex = 0; ChunkIndex < ChunkBoxesToDraw; ++ChunkIndex)
	{
		const FDRVoxelTerrainChunkBounds& Chunk = TerrainChunks[ChunkIndex];

		DrawDebugBox(
			World,
			Chunk.BoxCenter,
			Chunk.BoxExtent,
			TerrainChunkBoxColor,
			false,
			0.f,
			0,
			TerrainChunkBoxThickness);
	}
}
