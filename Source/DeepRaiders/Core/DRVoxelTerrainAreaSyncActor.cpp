#include "DRVoxelTerrainAreaSyncActor.h"

#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

namespace
{
	// 에디터에는 세부 반복 횟수 대신 세 가지 성능 단계만 노출한다. 실제 수치는 한 곳에서 관리해
	// 복셀 스캔, 복셀 쓰기, 고정 메시 트레이스의 부하 수준이 함께 움직이도록 한다.
	struct FDRDepositTickBudgets
	{
		FDRDepositTickBudgets(
			int32 InScanColumns,
			int32 InVoxelWriteAttempts,
			int32 InStaticMeshTraces)
			: ScanColumns(InScanColumns)
			, VoxelWriteAttempts(InVoxelWriteAttempts)
			, StaticMeshTraces(InStaticMeshTraces)
		{
		}

		int32 ScanColumns;
		int32 VoxelWriteAttempts;
		int32 StaticMeshTraces;
	};

	FDRDepositTickBudgets GetDepositTickBudgets(EDRDepositPerformancePreset Preset)
	{
		switch (Preset)
		{
		case EDRDepositPerformancePreset::Low:
			return FDRDepositTickBudgets(8, 32, 8);
		case EDRDepositPerformancePreset::High:
			return FDRDepositTickBudgets(64, 256, 64);
		case EDRDepositPerformancePreset::Balanced:
		default:
			return FDRDepositTickBudgets(32, 128, 32);
		}
	}

	constexpr float DRStaticMeshSampleJitterRatio = 0.4f;

	// 서버가 Revision 오름차순으로 추가한 배열에서 주어진 Revision보다 큰 첫 항목을 찾는다.
	// 전체 히스토리를 매 Tick 선형 순회하지 않기 위한 upper-bound 이진 탐색이다.
	template<typename RecordType>
	int32 FindFirstRecordAfterRevision(const TArray<RecordType>& Records, int32 Revision)
	{
		int32 MinIndex = 0;
		int32 MaxIndex = Records.Num();
		while (MinIndex < MaxIndex)
		{
			const int32 MiddleIndex = MinIndex + (MaxIndex - MinIndex) / 2;
			if (Records[MiddleIndex].Revision <= Revision)
			{
				MinIndex = MiddleIndex + 1;
			}
			else
			{
				MaxIndex = MiddleIndex;
			}
		}

		return MinIndex;
	}

	template<typename RecordType>
	bool HasRecordAfterRevision(const TArray<RecordType>& Records, int32 Revision)
	{
		return Records.Num() > 0 && Records.Last().Revision > Revision;
	}
}

ADRVoxelTerrainAreaSyncActor::ADRVoxelTerrainAreaSyncActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// 이 액터가 직접 Revision과 델타 배열을 복제한다. 중도 난입 클라이언트도 전체 기록을 받아야 하므로
	// 항상 관련 액터로 유지하고, 휴면 상태로 들어가 새 델타 복제가 멈추지 않게 한다.
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
}

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

void ADRVoxelTerrainAreaSyncActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureTerrainChunksCurrent();

	// 클라이언트는 타이머로 스캔하거나 퇴적을 생성하지 않고 복제된 델타만 재생한다.
	if (!HasAuthority())
	{
		return;
	}

	// 서버는 자신의 월드에 편집을 직접 적용하므로 기존 기록을 재생하지 않는다.
	// 현재 Revision을 적용 완료 지점으로 맞춘 뒤 이후 편집만 새 기록으로 생성한다.
	LastAppliedTerrainRevision = TerrainEditRevision;
	BindTerrainDugDelegate();

	if (bEnableMaterialCountScan && ScanInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			ScanTimerHandle,
			this,
			&ThisClass::ScanVoxelArea,
			ScanInterval,
			true,
			0.f);
	}

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

	if (bDrawDebugBox)
	{
		DrawScanDebugBox();
	}

	if (bDrawDepositGridPoints)
	{
		DrawDepositGridPoints();
	}

	if (bDrawTerrainChunkBoxes)
	{
		DrawTerrainChunkBoxes();
	}

	// 서버와 클라이언트가 같은 복셀 편집을 동시에 계산하지 않도록 역할을 분리한다.
	// 서버는 원본 데이터를 변경하고 델타를 만들며, 클라이언트는 복제된 결과만 적용한다.
	if (HasAuthority())
	{
		if (!bEnableDepositAccumulation &&
			(bDepositPassActive || DepositRequests.Num() > 0 || bStaticMeshSurfaceScanActive))
		{
			CancelDepositPass();
		}

		ProcessStaticMeshSurfaceScan();
		ProcessServerDepositRequests();
		// 이전 청크가 이번 틱에 끝났다면 즉시 다음 청크 요청을 준비한다.
		// 실제 스캔/쓰기는 다음 Tick부터 시작되므로 한 프레임 예산은 여전히 한 청크에만 사용된다.
		StartNextDepositChunk();
	}
	else
	{
		const bool bHasPendingTerrainEdit =
			TerrainEditRevision > LastAppliedTerrainRevision ||
			HasRecordAfterRevision(DepositDeltaRecords, LastAppliedTerrainRevision) ||
			HasRecordAfterRevision(DigDeltaRecords, LastAppliedTerrainRevision);
		if (bHasPendingTerrainEdit)
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
	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, DepositDeltaRecords);
	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, DigDeltaRecords);
}

void ADRVoxelTerrainAreaSyncActor::OnRep_VoxelWorld()
{
	RebuildTerrainChunks();
	ApplyPendingDeltaRecords();
}

void ADRVoxelTerrainAreaSyncActor::OnRep_TerrainDeltaState()
{
	// 세 복제 프로퍼티가 같은 프레임에 도착한다는 보장은 없다.
	// Apply 함수가 누락 Revision을 확인하므로 현재 도착한 자료만으로 안전하게 재생을 시도한다.
	ApplyPendingDeltaRecords();
}

void ADRVoxelTerrainAreaSyncActor::ScanVoxelArea()
{
	// 통계는 권한 서버의 현재 지형을 기준으로 계산한다. 동기화 자체에는 필요하지 않은 선택적 진단 작업이다.
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
	}

	TMap<uint8, int32> MaterialCounts;
	int32 TotalCount = 0;

	const bool bSuccess = UDRVoxelTerrainQueryLibrary::GetMaterialCountsInBox(
		VoxelWorld,
		GetActorLocation(),
		BoxExtent,
		DepositSettings.SurfaceSampleSpacing,
		TeamMaterialIndices,
		MaterialCounts,
		TotalCount);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to scan voxel area."));
		return;
	}

	if (TotalCount <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("No matching solid voxels found in area."));
		return;
	}

	for (const TPair<uint8, int32>& Pair : MaterialCounts)
	{
		const uint8 MaterialIndex = Pair.Key;
		const int32 Count = Pair.Value;
		const float Percent =
			static_cast<float>(Count) /
			static_cast<float>(TotalCount) *
			100.f;

		UE_LOG(
			LogTemp,
			Log,
			TEXT("Material %d: Count=%d Percent=%.2f%%"),
			MaterialIndex,
			Count,
			Percent);
	}
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

	if (bDepositPassActive || DepositRequests.Num() > 0 || bStaticMeshSurfaceScanActive)
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

	// 한 패스에서 모든 청크를 정확히 한 번씩 처리하되 순서를 매번 섞는다.
	// 큰 맵의 한쪽이 항상 먼저 쌓여 보이는 현상을 줄이고, 패스 중간 상태도 공간적으로 분산시킨다.
	DepositPassNumber = DepositPassNumber == MAX_int32 ? 1 : DepositPassNumber + 1;
	DepositChunkOrder.SetNumUninitialized(TerrainChunks.Num());
	for (int32 ChunkIndex = 0; ChunkIndex < DepositChunkOrder.Num(); ++ChunkIndex)
	{
		DepositChunkOrder[ChunkIndex] = ChunkIndex;
	}

	FRandomStream ChunkOrderRandomStream(FMath::Rand());
	for (int32 OrderIndex = DepositChunkOrder.Num() - 1; OrderIndex > 0; --OrderIndex)
	{
		DepositChunkOrder.Swap(
			OrderIndex,
			ChunkOrderRandomStream.RandRange(0, OrderIndex));
	}

	NextDepositChunkOrderIndex = 0;
	ActiveDepositChunkIndex = INDEX_NONE;
	DepositPassWrittenVoxelPositions.Reset();
	DepositPassWrittenColumns.Reset();
	bDepositPassActive = true;
	StartNextDepositChunk();
}

void ADRVoxelTerrainAreaSyncActor::StartNextDepositChunk()
{
	if (!HasAuthority() || !bEnableDepositAccumulation || !bDepositPassActive ||
		DepositRequests.Num() > 0 || bStaticMeshSurfaceScanActive)
	{
		return;
	}

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		CancelDepositPass();
		return;
	}

	while (NextDepositChunkOrderIndex < DepositChunkOrder.Num())
	{
		const int32 ChunkIndex = DepositChunkOrder[NextDepositChunkOrderIndex++];
		if (!TerrainChunks.IsValidIndex(ChunkIndex))
		{
			continue;
		}

		FDRVoxelTerrainChunkBounds& Chunk = TerrainChunks[ChunkIndex];
		FDRVoxelDepositInBoxSettings RequestSettings = DepositSettings;
		// 요청 내부의 모든 랜덤 연산은 이 Seed에서 파생된다. 요청이 여러 틱에 걸쳐 처리돼도
		// 프레임 타이밍과 무관하게 같은 후보 순서와 선택 결과를 유지한다.
		RequestSettings.RandomSeed = FMath::Rand();

		FDRVoxelDepositInBoxRequest Request;
		bool bRequestCreated = UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
			VoxelWorld,
			Chunk.BoxCenter,
			Chunk.BoxExtent,
			RequestSettings,
			Request);
		if (bRequestCreated)
		{
			// 후보 중심은 Chunk.Box 범위가 소유하지만 실제 풋프린트는 퍼짐 반경만큼 이웃 청크로 넘어간다.
			// 전체 관리 BoxExtent에서 다시 잘라 외곽 경계 밖에는 퇴적 데이터가 생성되지 않게 한다.
			bRequestCreated = UDRVoxelTerrainQueryLibrary::ConfigureDepositRequestWriteBounds(
				Request,
				GetActorLocation(),
				BoxExtent);
		}
		if (!bRequestCreated)
		{
			// 잘못된 한 청크 때문에 전체 패스가 멈추지 않게 완료 처리하고 다음 청크를 시도한다.
			Chunk.LastProcessedPass = DepositPassNumber;
			UE_LOG(
				LogTemp,
				Verbose,
				TEXT("Failed to create deposit request for chunk (%d, %d)."),
				Chunk.ChunkCoordinate.X,
				Chunk.ChunkCoordinate.Y);
			continue;
		}

		ActiveDepositChunkIndex = ChunkIndex;
		// 이 포인터는 액터가 소유하는 패스 집합을 가리킨다. 라이브러리가 실제로 기록한 중심/지지 복셀을
		// 즉시 집합에 추가하므로 다음 청크는 경계 중첩 위치를 다시 누적하지 않는다.
		Request.SharedWrittenVoxelPositions = &DepositPassWrittenVoxelPositions;
		Request.SharedWrittenColumns = &DepositPassWrittenColumns;
		DepositRequests.Add(MoveTemp(Request));

		// 고정 메시 표면을 사용하는 경우 같은 청크 범위의 비동기 트레이스를 먼저 완료한다.
		// 수집한 히트를 요청 후보에 합친 뒤 복셀 표면 스캔이 이어서 전체 퍼센트를 함께 계산한다.
		if (bDepositOnStaticMeshes)
		{
			BeginStaticMeshSurfaceScan(RequestSettings, Chunk.BoxCenter, Chunk.BoxExtent);
		}
		return;
	}

	// 섞인 청크 배열의 끝까지 도달하면 한 번의 전체 맵 패스가 끝난다.
	bDepositPassActive = false;
	ActiveDepositChunkIndex = INDEX_NONE;
	NextDepositChunkOrderIndex = 0;
	DepositChunkOrder.Reset();
	DepositPassWrittenVoxelPositions.Reset();
	DepositPassWrittenColumns.Reset();
}

void ADRVoxelTerrainAreaSyncActor::CancelDepositPass()
{
	DepositRequests.Reset();
	CancelStaticMeshSurfaceScan();
	DepositChunkOrder.Reset();
	DepositPassWrittenVoxelPositions.Reset();
	DepositPassWrittenColumns.Reset();
	NextDepositChunkOrderIndex = 0;
	ActiveDepositChunkIndex = INDEX_NONE;
	bDepositPassActive = false;
}

void ADRVoxelTerrainAreaSyncActor::ProcessServerDepositRequests()
{
	if (!HasAuthority() || DepositRequests.Num() == 0)
	{
		return;
	}

	const FDRVoxelDepositInBoxRequest& ActiveRequest = DepositRequests[0];
	if (!bEnableDepositAccumulation ||
		!IsValid(VoxelWorld) ||
		!VoxelWorld->IsCreated() ||
		ActiveRequest.VoxelWorld.Get() != VoxelWorld)
	{
		// 기능이 꺼졌거나 대상 월드가 교체되면 이전 표면/월드를 기준으로 한 요청을 즉시 폐기한다.
		CancelDepositPass();
		return;
	}

	// 비동기 메시 트레이스가 끝나기 전에 복셀 요청이 완료되어 배열에서 제거되면 결과를 합칠 곳이 없어진다.
	// 따라서 메시 스캔 동안만 복셀 읽기/쓰기를 보류하고, 완료된 같은 틱부터 아래 상태 머신을 진행한다.
	if (bStaticMeshSurfaceScanActive)
	{
		return;
	}

	int32 ModifiedVoxelCount = 0;
	int32 ScannedColumnCount = 0;
	int32 RemainingRequestCount = 0;
	FDRVoxelDepositDeltaRecord DeltaRecord;
	const int32 ProcessedChunkIndex = ActiveDepositChunkIndex;
	const FDRDepositTickBudgets TickBudgets = GetDepositTickBudgets(PerformancePreset);

	// 라이브러리는 읽기 또는 쓰기 단계 중 하나를 지정 예산만큼만 수행한다.
	// 반환 bool 대신 출력 카운트와 DeltaRecord로 이번 틱에 실제 복제할 변경이 있는지 판단한다.
	UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
		DepositRequests,
		TickBudgets.ScanColumns,
		TickBudgets.VoxelWriteAttempts,
		ModifiedVoxelCount,
		ScannedColumnCount,
		DeltaRecord,
		RemainingRequestCount);

	// 요청이 배열에서 제거됐다는 것은 선택된 후보의 쓰기 시도까지 모두 끝났다는 뜻이다.
	// 실제 변경 수가 0이어도 해당 청크의 이번 패스 처리는 완료됐으므로 다음 청크로 넘어갈 수 있다.
	if (DepositRequests.Num() == 0)
	{
		if (TerrainChunks.IsValidIndex(ProcessedChunkIndex))
		{
			TerrainChunks[ProcessedChunkIndex].LastProcessedPass = DepositPassNumber;
		}
		ActiveDepositChunkIndex = INDEX_NONE;
	}

	if (ModifiedVoxelCount <= 0 || DeltaRecord.Deltas.Num() == 0)
	{
		return;
	}

	// 스캔만 했거나 모든 쓰기 시도가 실패한 틱에는 Revision을 소비하지 않는다.
	// 실제 서버 복셀 값이 바뀐 배치만 하나의 편집 단위로 번호를 부여하고 복제 배열에 추가한다.
	TerrainEditRevision++;
	DeltaRecord.Revision = TerrainEditRevision;
	DepositDeltaRecords.Add(MoveTemp(DeltaRecord));

	TrimReplicatedTerrainRecords();
	ForceNetUpdate();

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Deposit replicated. Revision=%d ModifiedVoxelCount=%d ScannedColumnCount=%d RemainingRequestCount=%d"),
		TerrainEditRevision,
		ModifiedVoxelCount,
		ScannedColumnCount,
		RemainingRequestCount);
}

bool ADRVoxelTerrainAreaSyncActor::BeginStaticMeshSurfaceScan(
	const FDRVoxelDepositInBoxSettings& RequestSettings,
	const FVector& ScanCenter,
	const FVector& ScanExtent)
{
	CancelStaticMeshSurfaceScan();

	UWorld* World = GetWorld();
	if (!HasAuthority() || !bDepositOnStaticMeshes ||
		!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!FMath::IsFinite(RequestSettings.SurfaceSampleSpacing) ||
		RequestSettings.SurfaceSampleSpacing <= 0.f ||
		ScanCenter.ContainsNaN() || ScanExtent.ContainsNaN())
	{
		return false;
	}

	const FVector AbsExtent(
		FMath::Abs(ScanExtent.X),
		FMath::Abs(ScanExtent.Y),
		FMath::Abs(ScanExtent.Z));
	if (AbsExtent.Z <= KINDA_SMALL_NUMBER ||
		!FMath::IsFinite(AbsExtent.X) ||
		!FMath::IsFinite(AbsExtent.Y) ||
		!FMath::IsFinite(AbsExtent.Z))
	{
		return false;
	}

	// 월드 공간 박스의 X/Y 격자를 사용한다. 개수 계산은 int64로 검증한 뒤에만 int32 배열을 만든다.
	const double SpanX = static_cast<double>(AbsExtent.X) * 2.0;
	const double SpanY = static_cast<double>(AbsExtent.Y) * 2.0;
	const double SampleSpacing = static_cast<double>(RequestSettings.SurfaceSampleSpacing);
	const int64 ColumnCountX = FMath::FloorToInt64(SpanX / SampleSpacing) + 1;
	const int64 ColumnCountY = FMath::FloorToInt64(SpanY / SampleSpacing) + 1;
	if (ColumnCountX <= 0 || ColumnCountY <= 0 ||
		ColumnCountX > MAX_int32 || ColumnCountY > MAX_int32 ||
		ColumnCountX > MAX_int32 / ColumnCountY)
	{
		UE_LOG(LogTemp, Warning, TEXT("Static mesh deposit trace grid exceeds the supported range."));
		return false;
	}

	const int32 TotalColumnCount = static_cast<int32>(ColumnCountX * ColumnCountY);
	StaticMeshTraceColumnOrder.SetNumUninitialized(TotalColumnCount);
	for (int32 Index = 0; Index < TotalColumnCount; ++Index)
	{
		StaticMeshTraceColumnOrder[Index] = Index;
	}

	// 복셀 후보와 다른 고정 상수를 섞어도 요청 Seed 하나로 전체 분포를 재현할 수 있다.
	StaticMeshTraceRandomStream.Initialize(RequestSettings.RandomSeed ^ 0x5A17C9E3);
	for (int32 Index = StaticMeshTraceColumnOrder.Num() - 1; Index > 0; --Index)
	{
		StaticMeshTraceColumnOrder.Swap(
			Index,
			StaticMeshTraceRandomStream.RandRange(0, Index));
	}

	StaticMeshScanVoxelWorld = VoxelWorld;
	StaticMeshScanCenter = ScanCenter;
	StaticMeshScanExtent = AbsExtent;
	StaticMeshTraceColumnCountY = static_cast<int32>(ColumnCountY);
	NextStaticMeshTraceColumnIndex = 0;
	bStaticMeshSurfaceScanActive = true;
	return true;
}

void ADRVoxelTerrainAreaSyncActor::ProcessStaticMeshSurfaceScan()
{
	if (!bStaticMeshSurfaceScanActive)
	{
		return;
	}

	if (!HasAuthority() || !bEnableDepositAccumulation || !bDepositOnStaticMeshes ||
		DepositRequests.Num() == 0 || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		StaticMeshScanVoxelWorld.Get() != VoxelWorld)
	{
		// 기능 비활성화는 메시 스캔만 취소하면 기존 요청이 복셀 전용으로 계속될 수 있다.
		// 전체 누적이 꺼졌거나 월드가 바뀐 경우에는 ProcessServerDepositRequests가 요청 배열도 정리한다.
		CancelStaticMeshSurfaceScan();
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		CancelStaticMeshSurfaceScan();
		return;
	}

	const FDRDepositTickBudgets TickBudgets = GetDepositTickBudgets(PerformancePreset);
	const float MinimumSurfaceNormalZ = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(MaxStaticMeshSlopeAngle, 0.f, 90.f)));

	// 결과는 요청 다음 프레임부터 유효하다. 완료되지 않은 핸들은 유지하고, 만료된 핸들은 버려
	// 오래된 비동기 결과가 다음 퇴적 요청에 섞이지 않게 한다.
	for (int32 HandleIndex = PendingStaticMeshTraceHandles.Num() - 1; HandleIndex >= 0; --HandleIndex)
	{
		const FTraceHandle TraceHandle = PendingStaticMeshTraceHandles[HandleIndex];
		FTraceDatum TraceData;
		if (World->QueryTraceData(TraceHandle, TraceData))
		{
			PendingStaticMeshTraceHandles.RemoveAtSwap(HandleIndex, 1, EAllowShrinking::No);

			for (const FHitResult& Hit : TraceData.OutHits)
			{
				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
				AActor* HitActor = Hit.GetActor();
				if (!IsValid(StaticMeshComponent) ||
					StaticMeshComponent->GetMobility() != EComponentMobility::Static ||
					Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() ||
					Hit.ImpactNormal.Z < MinimumSurfaceNormalZ)
				{
					continue;
				}

				const bool bTagMatches = RequiredStaticMeshSurfaceTag.IsNone() ||
					StaticMeshComponent->ComponentHasTag(RequiredStaticMeshSurfaceTag) ||
					(IsValid(HitActor) && HitActor->ActorHasTag(RequiredStaticMeshSurfaceTag));
				if (!bTagMatches)
				{
					continue;
				}

				// Single WorldStatic 트레이스의 첫 충돌이 VoxelWorld라면 Cast가 실패해 여기까지 오지 않는다.
				// 따라서 복셀 눈이나 지형이 메시보다 위에 있을 때 아래 메시를 중복 지지면으로 추가하지 않는다.
				StaticMeshSurfaceHitPositions.Add(Hit.ImpactPoint);
				break;
			}
		}
		else if (!World->IsTraceHandleValid(TraceHandle, false))
		{
			PendingStaticMeshTraceHandles.RemoveAtSwap(HandleIndex, 1, EAllowShrinking::No);
		}
	}

	const int32 TraceBudget = FMath::Max(1, TickBudgets.StaticMeshTraces);
	// 발행 예산의 두 프레임분만 비행 중 상태로 허용해 물리 쿼리가 밀릴 때 핸들이 끝없이 늘지 않게 한다.
	const int32 PendingTraceLimit = FMath::Max(1, TickBudgets.StaticMeshTraces * 2);
	int32 IssuedTraceCount = 0;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DRStaticMeshDepositSurface),
		bTraceComplexStaticMeshSurfaces);
	QueryParams.AddIgnoredActor(this);

	const FVector BoxMin = StaticMeshScanCenter - StaticMeshScanExtent;
	const FVector BoxMax = StaticMeshScanCenter + StaticMeshScanExtent;
	const FDRVoxelDepositInBoxSettings& ActiveDepositSettings =
		DepositRequests[0].DepositSettings;
	const float JitterRadius =
		ActiveDepositSettings.SurfaceSampleSpacing * DRStaticMeshSampleJitterRatio;

	while (IssuedTraceCount < TraceBudget &&
		PendingStaticMeshTraceHandles.Num() < PendingTraceLimit &&
		NextStaticMeshTraceColumnIndex < StaticMeshTraceColumnOrder.Num())
	{
		const int32 LinearIndex = StaticMeshTraceColumnOrder[NextStaticMeshTraceColumnIndex++];
		const int32 ColumnX = LinearIndex / StaticMeshTraceColumnCountY;
		const int32 ColumnY = LinearIndex % StaticMeshTraceColumnCountY;
		const float BaseX = BoxMin.X + ColumnX * ActiveDepositSettings.SurfaceSampleSpacing;
		const float BaseY = BoxMin.Y + ColumnY * ActiveDepositSettings.SurfaceSampleSpacing;
		// Clamp로 경계선에 트레이스가 몰리지 않도록 현재 기준점에서 박스 안으로 허용되는 지터만 뽑는다.
		const float MinJitterX = FMath::Max(-JitterRadius, BoxMin.X - BaseX);
		const float MaxJitterX = FMath::Min(JitterRadius, BoxMax.X - BaseX);
		const float MinJitterY = FMath::Max(-JitterRadius, BoxMin.Y - BaseY);
		const float MaxJitterY = FMath::Min(JitterRadius, BoxMax.Y - BaseY);
		const float SampleX = BaseX + StaticMeshTraceRandomStream.FRandRange(MinJitterX, MaxJitterX);
		const float SampleY = BaseY + StaticMeshTraceRandomStream.FRandRange(MinJitterY, MaxJitterY);

		const FTraceHandle TraceHandle = World->AsyncLineTraceByObjectType(
			EAsyncTraceType::Single,
			FVector(SampleX, SampleY, BoxMax.Z),
			FVector(SampleX, SampleY, BoxMin.Z),
			ObjectQueryParams,
			QueryParams);
		if (TraceHandle.IsValid())
		{
			PendingStaticMeshTraceHandles.Add(TraceHandle);
		}
		IssuedTraceCount++;
	}

	if (NextStaticMeshTraceColumnIndex >= StaticMeshTraceColumnOrder.Num() &&
		PendingStaticMeshTraceHandles.Num() == 0)
	{
		FinishStaticMeshSurfaceScan();
	}
}

void ADRVoxelTerrainAreaSyncActor::FinishStaticMeshSurfaceScan()
{
	if (!bStaticMeshSurfaceScanActive || DepositRequests.Num() == 0)
	{
		CancelStaticMeshSurfaceScan();
		return;
	}

	FDRVoxelDepositInBoxRequest& Request = DepositRequests[0];
	if (!Request.bIsValid || Request.VoxelWorld.Get() != VoxelWorld)
	{
		CancelStaticMeshSurfaceScan();
		return;
	}

	if (StaticMeshSurfaceHitPositions.Num() > 0)
	{
		// 비동기 완료 순서는 물리 작업 스케줄에 따라 달라질 수 있다. 좌표 순서로 정렬해 요청에 넣으면
		// 라이브러리의 Seed 기반 가중 선택 결과가 트레이스 완료 프레임 순서에 영향을 받지 않는다.
		StaticMeshSurfaceHitPositions.Sort([](const FVector& A, const FVector& B)
		{
			if (A.X != B.X)
			{
				return A.X < B.X;
			}
			if (A.Y != B.Y)
			{
				return A.Y < B.Y;
			}
			return A.Z < B.Z;
		});
	}

	int32 AddedCandidateCount = 0;
	UDRVoxelTerrainQueryLibrary::AddExternalSurfaceDepositCandidates(
		Request,
		StaticMeshSurfaceHitPositions,
		AddedCandidateCount);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Static mesh deposit scan completed. HitCount=%d AddedCandidateCount=%d"),
		StaticMeshSurfaceHitPositions.Num(),
		AddedCandidateCount);

	CancelStaticMeshSurfaceScan();
}

void ADRVoxelTerrainAreaSyncActor::CancelStaticMeshSurfaceScan()
{
	// 엔진의 비동기 트레이스는 명시적으로 취소하지 않아도 다음 프레임 버퍼에서 만료된다.
	// 핸들을 버리면 완료 결과를 조회하지 않으므로 폐기된 요청의 표면이 새 요청에 들어오지 않는다.
	PendingStaticMeshTraceHandles.Reset();
	StaticMeshSurfaceHitPositions.Reset();
	StaticMeshTraceColumnOrder.Reset();
	StaticMeshScanVoxelWorld.Reset();
	StaticMeshScanCenter = FVector::ZeroVector;
	StaticMeshScanExtent = FVector::ZeroVector;
	StaticMeshTraceColumnCountY = 0;
	NextStaticMeshTraceColumnIndex = 0;
	bStaticMeshSurfaceScanActive = false;
}

void ADRVoxelTerrainAreaSyncActor::ApplyPendingDeltaRecords()
{
	if (HasAuthority() || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return;
	}

	// 배열은 서버에서 Revision 오름차순으로 추가된다. 전체 기록을 매 Tick 처음부터 순회하지 않고
	// 이진 탐색으로 LastApplied 바로 다음 위치를 찾아 히스토리가 커져도 탐색 비용을 낮게 유지한다.
	int32 DepositIndex = FindFirstRecordAfterRevision(
		DepositDeltaRecords,
		LastAppliedTerrainRevision);
	int32 DigIndex = FindFirstRecordAfterRevision(
		DigDeltaRecords,
		LastAppliedTerrainRevision);

	// DepositDeltaRecords와 DigDeltaRecords는 별도 프로퍼티라 도착 순서가 보장되지 않는다.
	// 두 배열의 다음 항목 중 Revision이 작은 것을 선택해 하나의 서버 편집 흐름처럼 병합한다.
	while (true)
	{
		const FDRVoxelDepositDeltaRecord* DepositRecord =
			DepositIndex < DepositDeltaRecords.Num()
				? &DepositDeltaRecords[DepositIndex]
				: nullptr;
		const FDRVoxelDigDeltaRecord* DigRecord =
			DigIndex < DigDeltaRecords.Num()
				? &DigDeltaRecords[DigIndex]
				: nullptr;

		if (!DepositRecord && !DigRecord)
		{
			const int32 ExpectedRevision = LastAppliedTerrainRevision + 1;
			// 서버 Revision은 앞서 있지만 해당 레코드가 아직 없으면 다른 프로퍼티의 복제가 늦은 경우일 수 있다.
			// 이후 편집을 먼저 적용하지 않고 다음 OnRep 또는 Tick까지 기다려 지형 결과가 뒤집히는 것을 막는다.
			if (TerrainEditRevision >= ExpectedRevision &&
				LastReportedMissingRevision != ExpectedRevision)
			{
				LastReportedMissingRevision = ExpectedRevision;
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Terrain delta unavailable. Expected=%d ServerRevision=%d."),
					ExpectedRevision,
					TerrainEditRevision);
			}
			return;
		}

		// 서버 편집은 반드시 연속 Revision으로 적용한다. 예를 들어 5가 없는데 6을 먼저 적용하면
		// 5가 나중에 도착했을 때 쌓기/굴착의 최종 결과가 서버와 달라질 수 있다.
		const int32 ExpectedRevision = LastAppliedTerrainRevision + 1;
		const bool bApplyDeposit =
			DepositRecord && (!DigRecord || DepositRecord->Revision < DigRecord->Revision);
		const int32 NextRevision = bApplyDeposit
			? DepositRecord->Revision
			: DigRecord->Revision;

		if (NextRevision != ExpectedRevision)
		{
			if (LastReportedMissingRevision != ExpectedRevision)
			{
				LastReportedMissingRevision = ExpectedRevision;
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Terrain delta revision gap. Expected=%d Received=%d. Later deltas will wait."),
					ExpectedRevision,
					NextRevision);
			}
			return;
		}

		// 두 배열 중 더 이른 Revision의 타입에 맞는 재생 함수를 호출한다.
		// 재생이 실패하면 LastApplied를 전진시키지 않아 월드가 준비된 다음 호출에서 다시 시도할 수 있다.
		bool bApplied = false;
		if (bApplyDeposit)
		{
			int32 AppliedVoxelCount = 0;
			bApplied = UDRVoxelTerrainQueryLibrary::ApplyDepositDeltaRecord(
				VoxelWorld,
				*DepositRecord,
				AppliedVoxelCount);
			DepositIndex++;
		}
		else
		{
			bApplied = UDRVoxelTerrainQueryLibrary::ApplyDigDeltaRecord(
				VoxelWorld,
				*DigRecord);
			DigIndex++;
		}

		if (!bApplied)
		{
			return;
		}

		// 성공한 경우에만 재생 커서를 전진시키고 이전 누락 경고 상태를 해제한다.
		LastAppliedTerrainRevision = NextRevision;
		LastReportedMissingRevision = 0;
	}
}

void ADRVoxelTerrainAreaSyncActor::TrimReplicatedTerrainRecords()
{
	if (MaxReplicatedDeltaRecords <= 0)
	{
		return;
	}

	// 배열별 개수가 아니라 공통 Revision 범위로 잘라야 퇴적과 굴착 사이의 상대 순서가 유지된다.
	// 단, 기록을 제거한 뒤 접속한 클라이언트는 누락분을 복원할 수 없으므로 체크포인트가 있을 때만 제한값을 사용한다.
	const int32 MinimumRevisionToKeep = FMath::Max(
		1,
		TerrainEditRevision - MaxReplicatedDeltaRecords + 1);
	const int32 DepositRecordsToRemove = FindFirstRecordAfterRevision(
		DepositDeltaRecords,
		MinimumRevisionToKeep - 1);
	const int32 DigRecordsToRemove = FindFirstRecordAfterRevision(
		DigDeltaRecords,
		MinimumRevisionToKeep - 1);

	if (DepositRecordsToRemove > 0)
	{
		DepositDeltaRecords.RemoveAt(0, DepositRecordsToRemove, EAllowShrinking::No);
	}
	if (DigRecordsToRemove > 0)
	{
		DigDeltaRecords.RemoveAt(0, DigRecordsToRemove, EAllowShrinking::No);
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
	if (!UDRVoxelTerrainQueryLibrary::IsVoxelUpdateInBox(
		GetActorLocation(),
		BoxExtent,
		Location,
		Radius))
	{
		return;
	}

	// 진행 중인 요청의 후보 표면은 굴착 이전 높이를 기준으로 계산됐을 수 있다.
	// 그대로 쓰면 파낸 공간 위에 오래된 후보가 쌓일 수 있으므로 현재 청크뿐 아니라 남은 패스 순서와
	// 비동기 메시 트레이스까지 함께 폐기한다. 다음 타이머 주기에는 변경된 지형을 기준으로 새 패스를 만든다.
	CancelDepositPass();

	// 이 델리게이트는 서버의 실제 굴착이 끝난 뒤 호출된다. 여기서 서버 지형을 다시 수정하지 않고
	// 클라이언트 재생에 필요한 위치, 반지름, Revision만 기록해 중복 굴착을 방지한다.
	FDRVoxelDigDeltaRecord DigRecord;
	DigRecord.Revision = ++TerrainEditRevision;
	DigRecord.Location = Location;
	DigRecord.Radius = Radius;
	DigDeltaRecords.Add(DigRecord);

	TrimReplicatedTerrainRecords();
	ForceNetUpdate();
}

void ADRVoxelTerrainAreaSyncActor::DrawScanDebugBox() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	DrawDebugBox(
		World,
		GetActorLocation(),
		BoxExtent,
		FColor::Cyan,
		false,
		0.f,
		0,
		3.f);
}

void ADRVoxelTerrainAreaSyncActor::DrawDepositGridPoints() const
{
	UWorld* World = GetWorld();
	const float GridStep = DepositSettings.SurfaceSampleSpacing;
	if (!IsValid(World) || GridStep <= 0.f || MaxDebugDepositGridPoints <= 0)
	{
		return;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.IsNearlyZero())
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const FVector Min = Center - AbsExtent;
	const FVector Max = Center + AbsExtent;

	int32 DrawnPointCount = 0;

	// 이 표시는 SurfaceSampleSpacing의 기준 3D 격자다. 실제 후보 스캔은 로컬 복셀 정수 간격과 X/Y 지터를 사용하므로
	// 점은 처리 밀도를 이해하기 위한 참고용이며 실제 퇴적 위치를 정확히 나타내지는 않는다.
	for (float X = Min.X; X <= Max.X && DrawnPointCount < MaxDebugDepositGridPoints; X += GridStep)
	{
		for (float Y = Min.Y; Y <= Max.Y && DrawnPointCount < MaxDebugDepositGridPoints; Y += GridStep)
		{
			for (float Z = Min.Z; Z <= Max.Z && DrawnPointCount < MaxDebugDepositGridPoints; Z += GridStep)
			{
				DrawDebugPoint(
					World,
					FVector(X, Y, Z),
					DepositGridPointSize,
					DepositGridPointColor,
					false,
					0.f);

				DrawnPointCount++;
			}
		}
	}
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
