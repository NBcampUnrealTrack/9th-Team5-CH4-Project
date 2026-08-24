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
	void GetLocalVoxelBoundsForWorldBox(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& AbsExtent,
		FIntVector& OutVoxelMin,
		FIntVector& OutVoxelMax)
	{
		bool bHasBounds = false;
		for (int32 SignX = -1; SignX <= 1; SignX += 2)
		{
			for (int32 SignY = -1; SignY <= 1; SignY += 2)
			{
				for (int32 SignZ = -1; SignZ <= 1; SignZ += 2)
				{
					const FVector WorldCorner = BoxCenter + FVector(
						AbsExtent.X * SignX,
						AbsExtent.Y * SignY,
						AbsExtent.Z * SignZ);
					const FIntVector LocalCorner = VoxelWorld->GlobalToLocal(WorldCorner);

					if (!bHasBounds)
					{
						OutVoxelMin = LocalCorner;
						OutVoxelMax = LocalCorner;
						bHasBounds = true;
						continue;
					}

					OutVoxelMin.X = FMath::Min(OutVoxelMin.X, LocalCorner.X);
					OutVoxelMin.Y = FMath::Min(OutVoxelMin.Y, LocalCorner.Y);
					OutVoxelMin.Z = FMath::Min(OutVoxelMin.Z, LocalCorner.Z);
					OutVoxelMax.X = FMath::Max(OutVoxelMax.X, LocalCorner.X);
					OutVoxelMax.Y = FMath::Max(OutVoxelMax.Y, LocalCorner.Y);
					OutVoxelMax.Z = FMath::Max(OutVoxelMax.Z, LocalCorner.Z);
				}
			}
		}
	}

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
	// 청크는 영역의 로컬 복셀 경계를 보관한다. 마지막 청크는 전체 크기보다 작을 수 있으므로
	// VoxelMaxExclusive를 동기화 박스의 끝에 맞춰 잘라 낸다.
	TerrainChunks.Reset();
	bHasCachedChunkLayout = true;
	CachedChunkVoxelWorld = VoxelWorld;
	CachedChunkCenter = GetActorLocation();
	CachedChunkExtent = BoxExtent;
	CachedChunkSizeInVoxels = TerrainChunkSizeInVoxels;
	CachedChunkVoxelWorldTransform = FTransform::Identity;
	CachedChunkVoxelSize = 0.f;

	if (!IsValid(VoxelWorld) || TerrainChunkSizeInVoxels <= 0)
	{
		return;
	}

	CachedChunkVoxelWorldTransform = VoxelWorld->GetActorTransform();
	CachedChunkVoxelSize = VoxelWorld->VoxelSize;

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.IsNearlyZero())
	{
		return;
	}

	// 관리 박스를 VoxelWorld 로컬 복셀 좌표로 변환한다. MaxExclusive를 사용하면
	// 영역 크기, 청크 개수, 마지막 청크 자르기를 모두 뺄셈 기반으로 계산할 수 있다.
	const FVector Center = GetActorLocation();
	FIntVector RegionVoxelMin = FIntVector::ZeroValue;
	FIntVector RegionVoxelMaxInclusive = FIntVector::ZeroValue;
	GetLocalVoxelBoundsForWorldBox(
		VoxelWorld,
		Center,
		AbsExtent,
		RegionVoxelMin,
		RegionVoxelMaxInclusive);
	if (RegionVoxelMaxInclusive.X == MAX_int32 ||
		RegionVoxelMaxInclusive.Y == MAX_int32 ||
		RegionVoxelMaxInclusive.Z == MAX_int32)
	{
		return;
	}
	const FIntVector RegionVoxelMaxExclusive = RegionVoxelMaxInclusive + FIntVector(1);
	const int64 RegionSizeX = static_cast<int64>(RegionVoxelMaxExclusive.X) - RegionVoxelMin.X;
	const int64 RegionSizeY = static_cast<int64>(RegionVoxelMaxExclusive.Y) - RegionVoxelMin.Y;
	const int64 RegionSizeZ = static_cast<int64>(RegionVoxelMaxExclusive.Z) - RegionVoxelMin.Z;
	if (RegionSizeX <= 0 || RegionSizeY <= 0 || RegionSizeZ <= 0 ||
		RegionSizeX > MAX_int32 || RegionSizeY > MAX_int32 || RegionSizeZ > MAX_int32)
	{
		return;
	}
	const FIntVector RegionSize(
		static_cast<int32>(RegionSizeX),
		static_cast<int32>(RegionSizeY),
		static_cast<int32>(RegionSizeZ));
	// 영역 크기가 청크 크기의 배수가 아니면 마지막 청크가 하나 더 필요하므로 올림 나눗셈을 사용한다.
	const FIntVector ChunkCounts(
		FMath::DivideAndRoundUp(RegionSize.X, TerrainChunkSizeInVoxels),
		FMath::DivideAndRoundUp(RegionSize.Y, TerrainChunkSizeInVoxels),
		FMath::DivideAndRoundUp(RegionSize.Z, TerrainChunkSizeInVoxels));
	const int64 ChunkCountXY = static_cast<int64>(ChunkCounts.X) * ChunkCounts.Y;

	if (ChunkCountXY <= 0 || ChunkCountXY > MAX_int32 ||
		ChunkCounts.Z <= 0 || ChunkCountXY > MAX_int32 / ChunkCounts.Z)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Terrain chunk count exceeds the supported range."));
		return;
	}
	const int32 TotalChunkCount = static_cast<int32>(ChunkCountXY * ChunkCounts.Z);

	TerrainChunks.Reserve(TotalChunkCount);

	// 각 청크는 정규 크기로 시작하지만 영역 끝을 넘는 축은 RegionVoxelMaxExclusive에서 잘라 낸다.
	for (int32 ChunkX = 0; ChunkX < ChunkCounts.X; ++ChunkX)
	{
		for (int32 ChunkY = 0; ChunkY < ChunkCounts.Y; ++ChunkY)
		{
			for (int32 ChunkZ = 0; ChunkZ < ChunkCounts.Z; ++ChunkZ)
			{
				FDRVoxelTerrainChunkBounds Chunk;
				Chunk.ChunkCoordinate = FIntVector(ChunkX, ChunkY, ChunkZ);
				Chunk.VoxelMin = RegionVoxelMin + FIntVector(
					ChunkX * TerrainChunkSizeInVoxels,
					ChunkY * TerrainChunkSizeInVoxels,
					ChunkZ * TerrainChunkSizeInVoxels);
				Chunk.VoxelMaxExclusive = FIntVector(
					static_cast<int32>(FMath::Min(
						static_cast<int64>(Chunk.VoxelMin.X) + TerrainChunkSizeInVoxels,
						static_cast<int64>(RegionVoxelMaxExclusive.X))),
					static_cast<int32>(FMath::Min(
						static_cast<int64>(Chunk.VoxelMin.Y) + TerrainChunkSizeInVoxels,
						static_cast<int64>(RegionVoxelMaxExclusive.Y))),
					static_cast<int32>(FMath::Min(
						static_cast<int64>(Chunk.VoxelMin.Z) + TerrainChunkSizeInVoxels,
						static_cast<int64>(RegionVoxelMaxExclusive.Z))));

				TerrainChunks.Add(Chunk);
			}
		}
	}
}

void ADRVoxelTerrainAreaSyncActor::EnsureTerrainChunksCurrent()
{
	if (!IsValid(VoxelWorld))
	{
		if (!bHasCachedChunkLayout || TerrainChunks.Num() > 0 || CachedChunkVoxelWorld.IsValid())
		{
			RebuildTerrainChunks();
		}
		return;
	}

	// 액터 위치뿐 아니라 VoxelWorld의 변환, VoxelSize, 참조 교체까지 비교한다.
	// 어느 하나라도 달라지면 로컬 복셀 좌표와 월드 디버그 박스가 달라질 수 있다.
	const bool bLayoutIsCurrent =
		bHasCachedChunkLayout &&
		CachedChunkVoxelWorld.Get() == VoxelWorld &&
		CachedChunkCenter.Equals(GetActorLocation()) &&
		CachedChunkExtent.Equals(BoxExtent) &&
		CachedChunkSizeInVoxels == TerrainChunkSizeInVoxels &&
		FMath::IsNearlyEqual(CachedChunkVoxelSize, VoxelWorld->VoxelSize) &&
		CachedChunkVoxelWorldTransform.Equals(VoxelWorld->GetActorTransform());

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
	CancelStaticMeshSurfaceScan();
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
		ProcessStaticMeshSurfaceScan();
		ProcessServerDepositRequests();
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
		DepositSettings.SampleStep,
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
		// 기능을 끄는 즉시 이전 표면을 기준으로 만들어진 진행 중 요청도 폐기한다.
		DepositRequests.Reset();
		CancelStaticMeshSurfaceScan();
		return;
	}

	if (DepositRequests.Num() > 0)
	{
		// 여러 전체 스캔이 겹치면 같은 표면을 중복 후보로 만들 수 있다.
		// 현재 요청이 끝난 뒤에만 다음 주기 요청을 받아 순차 처리한다.
		return;
	}

	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		DepositRequests.Reset();
		CancelStaticMeshSurfaceScan();
		return;
	}

	// 에디터 설정은 유지하면서 요청마다 Seed만 바꾼다. 매 주기 분포는 달라지지만
	// 생성된 요청 내부에서는 고정 Seed를 사용해 틱 수와 관계없이 결정적인 순서를 유지한다.
	FDRVoxelDepositInBoxSettings RequestSettings = DepositSettings;
	RequestSettings.RandomSeed = FMath::Rand();

	FDRVoxelDepositInBoxRequest Request;
	const bool bRequestCreated = UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
		VoxelWorld,
		GetActorLocation(),
		BoxExtent,
		RequestSettings,
		Request);

	if (!bRequestCreated)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Failed to create deposit request."));
		return;
	}

	DepositRequests.Add(MoveTemp(Request));

	// 고정 메시 표면을 사용하는 경우에는 복셀 스캔을 바로 시작하지 않는다. 먼저 같은 요청 범위에
	// 비동기 하향 트레이스를 발행하고, 모든 히트를 후보로 합친 뒤 기존 상태 머신을 진행한다.
	if (bDepositOnStaticMeshes)
	{
		BeginStaticMeshSurfaceScan(RequestSettings);
	}
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
		ActiveRequest.VoxelWorld.Get() != VoxelWorld)
	{
		// 기능이 꺼졌거나 대상 월드가 교체되면 이전 표면/월드를 기준으로 한 요청을 즉시 폐기한다.
		DepositRequests.Reset();
		CancelStaticMeshSurfaceScan();
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

	// 라이브러리는 읽기 또는 쓰기 단계 중 하나를 지정 예산만큼만 수행한다.
	// 반환 bool 대신 출력 카운트와 DeltaRecord로 이번 틱에 실제 복제할 변경이 있는지 판단한다.
	UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
		DepositRequests,
		MaxDepositScanColumnsPerTick,
		MaxDepositVoxelWriteAttemptsPerTick,
		ModifiedVoxelCount,
		ScannedColumnCount,
		DeltaRecord,
		RemainingRequestCount);

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
	const FDRVoxelDepositInBoxSettings& RequestSettings)
{
	CancelStaticMeshSurfaceScan();

	UWorld* World = GetWorld();
	if (!HasAuthority() || !bDepositOnStaticMeshes ||
		!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!FMath::IsFinite(RequestSettings.SampleStep) || RequestSettings.SampleStep <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));
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
	const double SampleStep = static_cast<double>(RequestSettings.SampleStep);
	const int64 ColumnCountX = FMath::FloorToInt64(SpanX / SampleStep) + 1;
	const int64 ColumnCountY = FMath::FloorToInt64(SpanY / SampleStep) + 1;
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
	StaticMeshScanCenter = GetActorLocation();
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
					Hit.ImpactNormal.Z < FMath::Clamp(MinStaticMeshSurfaceNormalZ, -1.f, 1.f))
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

	const int32 TraceBudget = FMath::Max(1, MaxStaticMeshTraceRequestsPerTick);
	const int32 PendingTraceLimit = FMath::Max(1, MaxPendingStaticMeshTraces);
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
	const float JitterRadius = ActiveDepositSettings.bUseJitteredSamples
		? ActiveDepositSettings.SampleStep *
			FMath::Clamp(ActiveDepositSettings.JitterRatio, 0.f, 1.f)
		: 0.f;

	while (IssuedTraceCount < TraceBudget &&
		PendingStaticMeshTraceHandles.Num() < PendingTraceLimit &&
		NextStaticMeshTraceColumnIndex < StaticMeshTraceColumnOrder.Num())
	{
		const int32 LinearIndex = StaticMeshTraceColumnOrder[NextStaticMeshTraceColumnIndex++];
		const int32 ColumnX = LinearIndex / StaticMeshTraceColumnCountY;
		const int32 ColumnY = LinearIndex % StaticMeshTraceColumnCountY;
		const float BaseX = BoxMin.X + ColumnX * ActiveDepositSettings.SampleStep;
		const float BaseY = BoxMin.Y + ColumnY * ActiveDepositSettings.SampleStep;
		const float SampleX = FMath::Clamp(
			BaseX + StaticMeshTraceRandomStream.FRandRange(-JitterRadius, JitterRadius),
			BoxMin.X,
			BoxMax.X);
		const float SampleY = FMath::Clamp(
			BaseY + StaticMeshTraceRandomStream.FRandRange(-JitterRadius, JitterRadius),
			BoxMin.Y,
			BoxMax.Y);

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

	TArray<FVector> SelectedSurfacePositions;
	if (StaticMeshSurfaceHitPositions.Num() > 0)
	{
		// 비동기 완료 순서는 물리 작업 스케줄에 따라 달라질 수 있다. 좌표 순서로 정렬한 뒤 별도 Seed를
		// 사용해야 같은 입력에서 낮은 표면 선택 확률이 프레임 타이밍에 영향을 받지 않는다.
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

		float MinSurfaceZ = StaticMeshSurfaceHitPositions[0].Z;
		float MaxSurfaceZ = StaticMeshSurfaceHitPositions[0].Z;
		for (const FVector& HitPosition : StaticMeshSurfaceHitPositions)
		{
			MinSurfaceZ = FMath::Min(MinSurfaceZ, HitPosition.Z);
			MaxSurfaceZ = FMath::Max(MaxSurfaceZ, HitPosition.Z);
		}

		const float MinChance = FMath::Min(
			Request.DepositSettings.MinSurfaceDepositChance,
			Request.DepositSettings.MaxSurfaceDepositChance);
		const float MaxChance = FMath::Max(
			Request.DepositSettings.MinSurfaceDepositChance,
			Request.DepositSettings.MaxSurfaceDepositChance);
		const float HeightRange = MaxSurfaceZ - MinSurfaceZ;
		FRandomStream SelectionStream(Request.DepositSettings.RandomSeed ^ 0x234F19A7);

		SelectedSurfacePositions.Reserve(StaticMeshSurfaceHitPositions.Num());
		for (const FVector& HitPosition : StaticMeshSurfaceHitPositions)
		{
			const float LowerSurfaceAlpha = HeightRange > KINDA_SMALL_NUMBER
				? (MaxSurfaceZ - HitPosition.Z) / HeightRange
				: 1.f;
			const float BiasedLowerSurfaceAlpha = FMath::Pow(
				FMath::Clamp(LowerSurfaceAlpha, 0.f, 1.f),
				FMath::Max(0.01f, Request.DepositSettings.LowerSurfaceSelectionBias));
			const float DepositChance = FMath::Lerp(
				MinChance,
				MaxChance,
				BiasedLowerSurfaceAlpha);

			if (SelectionStream.FRand() <= DepositChance)
			{
				SelectedSurfacePositions.Add(HitPosition);
			}
		}
	}

	int32 AddedCandidateCount = 0;
	UDRVoxelTerrainQueryLibrary::AddExternalSurfaceDepositCandidates(
		Request,
		SelectedSurfacePositions,
		AddedCandidateCount);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Static mesh deposit scan completed. HitCount=%d SelectedCount=%d AddedCandidateCount=%d"),
		StaticMeshSurfaceHitPositions.Num(),
		SelectedSurfacePositions.Num(),
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
	// 그대로 쓰면 파낸 공간 위에 오래된 후보가 쌓일 수 있으므로 요청을 폐기하고 다음 주기에 다시 스캔한다.
	DepositRequests.Reset();

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
	const float GridStep = DepositSettings.SampleStep;
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

	// 이 표시는 SampleStep의 기준 3D 격자다. 실제 후보 스캔은 로컬 복셀 정수 간격과 X/Y 지터를 사용하므로
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
	if (!IsValid(World) || !IsValid(VoxelWorld) || MaxDebugTerrainChunkBoxes <= 0)
	{
		return;
	}

	// 청크 경계는 VoxelWorld 로컬 좌표로 저장된다. 중심은 LocalToGlobal로 변환하고,
	// 크기는 복셀 개수 * VoxelSize * 월드 스케일의 절반으로 계산해 회전된 VoxelWorld도 맞게 표시한다.
	const FVector VoxelWorldScale = VoxelWorld->GetActorScale3D().GetAbs();
	const FQuat VoxelWorldRotation = VoxelWorld->GetActorQuat();
	const int32 ChunkBoxesToDraw = FMath::Min(TerrainChunks.Num(), MaxDebugTerrainChunkBoxes);

	for (int32 ChunkIndex = 0; ChunkIndex < ChunkBoxesToDraw; ++ChunkIndex)
	{
		const FDRVoxelTerrainChunkBounds& Chunk = TerrainChunks[ChunkIndex];
		const FIntVector ChunkVoxelSize = Chunk.VoxelMaxExclusive - Chunk.VoxelMin;
		// VoxelMaxExclusive는 포함되지 않으므로 실제 첫/마지막 복셀 중심의 평균을 구할 때 OneVector를 뺀다.
		const FVector ChunkCenterInVoxelSpace =
			(FVector(Chunk.VoxelMin) + FVector(Chunk.VoxelMaxExclusive) - FVector::OneVector) * 0.5f;
		const FVector ChunkWorldCenter = VoxelWorld->LocalToGlobalFloatBP(ChunkCenterInVoxelSpace);
		const FVector ChunkWorldExtent =
			FVector(ChunkVoxelSize) * VoxelWorld->VoxelSize * 0.5f * VoxelWorldScale;

		DrawDebugBox(
			World,
			ChunkWorldCenter,
			ChunkWorldExtent,
			VoxelWorldRotation,
			TerrainChunkBoxColor,
			false,
			0.f,
			0,
			TerrainChunkBoxThickness);
	}
}
