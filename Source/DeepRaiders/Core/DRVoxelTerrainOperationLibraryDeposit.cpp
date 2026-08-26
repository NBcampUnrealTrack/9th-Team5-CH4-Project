#include "DRVoxelTerrainOperationLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "VoxelWorld.h"

namespace
{
	// 세부 반복 횟수를 에디터에 직접 노출하면 설정 조합이 빠르게 늘어난다.
	// 프리셋 하나로 복셀 열 스캔, 실제 복셀 쓰기, 물리 트레이스 예산을 함께 조절해
	// 세 단계 중 하나만 과도하게 앞서거나 프레임 비용이 한쪽으로 몰리지 않게 한다.
	struct FDRDepositTickBudgets
	{
		int32 ScanColumns = 0;
		int32 VoxelWriteAttempts = 0;
		int32 StaticMeshTraces = 0;
	};

	FDRDepositTickBudgets GetDepositTickBudgets(EDRDepositPerformancePreset Preset)
	{
		switch (Preset)
		{
		case EDRDepositPerformancePreset::Low:
			return {8, 32, 8};
		case EDRDepositPerformancePreset::High:
			return {64, 256, 64};
		case EDRDepositPerformancePreset::Balanced:
		default:
			return {32, 128, 32};
		}
	}

	constexpr float DRStaticMeshSampleJitterRatio = 0.4f;
	constexpr float DRStaticMeshFootprintEdgeStrength = 0.55f;
	// 메시와 복셀 표면이 수학적으로 같은 높이에 있을 때 부동소수점 오차로 틈이 보일 수 있다.
	// 지지면을 복셀 한 칸의 10%만큼 메시 안쪽으로 넣어 접촉면을 안정적으로 겹친다.
	constexpr float DRStaticMeshContactInsetVoxels = 0.1f;

	void DiscardStaticMeshFootprintData(FDRVoxelDepositInBoxRequest& Request)
	{
		// 정밀 트레이스를 계속할 수 없는 경우 부분 결과를 남기면 ResolveFootprints가
		// 불완전한 표면을 정상 결과로 사용할 수 있다. 외부 자료를 전부 비우고 완료 플래그를 세워
		// 복셀 지형 후보만으로 요청이 계속 진행되게 한다.
		Request.ExternalCandidatePositions.Reset();
		Request.ExternalSupportSurfaceZByVoxel.Reset();
		Request.ExternalResolvedAmountScaleByVoxel.Reset();
		Request.bExternalFootprintsResolved = true;
	}

	bool IsEligibleStaticMeshDepositHit(
		const FHitResult& Hit,
		float MinimumSurfaceNormalZ,
		FName RequiredSurfaceTag)
	{
		// WorldStatic 채널에는 StaticMesh가 아닌 컴포넌트도 들어올 수 있으므로 타입과 Mobility를
		// 다시 검사한다. 경사와 태그 검사는 1차/2차 스캔에서 동일한 기준을 사용해야
		// 후보 중심은 허용됐지만 풋프린트는 거부되는 불일치를 줄일 수 있다.
		const UStaticMeshComponent* StaticMeshComponent =
			Cast<UStaticMeshComponent>(Hit.GetComponent());
		if (!IsValid(StaticMeshComponent) ||
			StaticMeshComponent->GetMobility() != EComponentMobility::Static ||
			Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() ||
			Hit.ImpactNormal.Z < MinimumSurfaceNormalZ)
		{
			return false;
		}

		const AActor* HitActor = Hit.GetActor();
		return RequiredSurfaceTag.IsNone() ||
			StaticMeshComponent->ComponentHasTag(RequiredSurfaceTag) ||
			(IsValid(HitActor) && HitActor->ActorHasTag(RequiredSurfaceTag));
	}
}

bool UDRVoxelTerrainOperationLibrary::IsDepositPassActive(
	const FDRVoxelTerrainOperationState& State)
{
	return State.IsActive();
}

bool UDRVoxelTerrainOperationLibrary::StartDepositPass(
	const FDRVoxelTerrainOperationContext& Context,
	FDRVoxelTerrainOperationState& State)
{
	return State.StartPass(Context);
}

FDRVoxelTerrainOperationTickResult UDRVoxelTerrainOperationLibrary::TickDeposit(
	const FDRVoxelTerrainOperationContext& Context,
	FDRVoxelTerrainOperationState& State)
{
	return State.Tick(Context);
}

void UDRVoxelTerrainOperationLibrary::CancelDeposit(FDRVoxelTerrainOperationState& State)
{
	State.Cancel();
}

bool FDRVoxelTerrainOperationState::IsActive() const
{
	// bPassActive만 검사하면 마지막 청크 처리 중인 비동기 스캔을 놓칠 수 있다.
	// 반대로 요청만 검사하면 청크 사이의 짧은 전환 구간에 새 패스가 중첩될 수 있다.
	return bPassActive || bHasActiveRequest || bSurfaceScanActive || bFootprintScanActive;
}

bool FDRVoxelTerrainOperationState::StartPass(
	const FDRVoxelTerrainOperationContext& Context)
{
	if (IsActive() || Context.TerrainChunks == nullptr || Context.TerrainChunks->Num() == 0)
	{
		return false;
	}

	// PassNumber는 청크가 이번 패스에서 처리됐는지 표시하는 디버그/상태 값이다.
	// 0은 초기 상태로 남겨 두기 위해 오버플로 직전에는 1로 되돌린다.
	PassNumber = PassNumber == MAX_int32 ? 1 : PassNumber + 1;
	ChunkOrder.SetNumUninitialized(Context.TerrainChunks->Num());
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkOrder.Num(); ++ChunkIndex)
	{
		ChunkOrder[ChunkIndex] = ChunkIndex;
	}

	// 패스마다 순서를 섞어 넓은 영역에서 항상 같은 모서리부터 눈이 쌓여 보이는 현상을 줄인다.
	FRandomStream ChunkOrderRandomStream(FMath::Rand());
	DRVoxelTerrain::ShuffleArray(ChunkOrder, ChunkOrderRandomStream);

	NextChunkOrderIndex = 0;
	ActiveChunkIndex = INDEX_NONE;
	PassWrittenVoxelPositions.Reset();
	PassWrittenColumns.Reset();
	bPassActive = true;
	StartNextChunk(Context);
	return IsActive();
}

FDRVoxelTerrainOperationTickResult FDRVoxelTerrainOperationState::Tick(
	const FDRVoxelTerrainOperationContext& Context)
{
	FDRVoxelTerrainOperationTickResult Result;
	if (!IsActive())
	{
		return Result;
	}

	if (!Context.bHasAuthority || !Context.bEnableDepositAccumulation ||
		!IsValid(Context.VoxelWorld) || !Context.VoxelWorld->IsCreated())
	{
		Cancel();
		Result.bCancelled = true;
		return Result;
	}

	// 순서가 중요하다. 1차 스캔 결과가 후보 빌드에 들어가고, 후보 빌드가 ResolveFootprints로
	// 넘어간 뒤에야 2차 스캔 대상을 만들 수 있다. 같은 Tick에 단계가 완료돼도 각 함수는
	// 자신의 활성 플래그와 Phase를 다시 검사하므로 허용된 만큼만 다음 단계로 진행한다.
	ProcessSurfaceScan(Context);
	ProcessFootprintScan(Context);
	ProcessRequest(Context, Result);

	if (bPassActive && !bHasActiveRequest &&
		!bSurfaceScanActive && !bFootprintScanActive)
	{
		// 완료된 청크의 델타는 Tick 결과로 Actor에 반환된 뒤 새 청크는 상태만 준비한다.
		// 새 청크의 실제 스캔/쓰기는 다음 Tick에 시작되어 두 청크의 예산과 배치가 섞이지 않는다.
		StartNextChunk(Context);
	}

	return Result;
}

void FDRVoxelTerrainOperationState::Cancel()
{
	// 취소는 재진입 가능해야 한다. 부분 초기화 순서에 의존하지 않도록 요청과 두 스캔,
	// 패스 공유 자료를 모두 기본 상태로 되돌린다. 비동기 TraceHandle은 배열에서 버리면
	// 이후 완료 결과를 조회하지 않으며, 결과가 작업 상태에 다시 유입될 경로도 사라진다.
	ActiveRequest = FDRVoxelDepositInBoxRequest();
	bHasActiveRequest = false;
	CancelSurfaceScan();
	CancelFootprintScan();
	ChunkOrder.Reset();
	PassWrittenVoxelPositions.Reset();
	PassWrittenColumns.Reset();
	NextChunkOrderIndex = 0;
	ActiveChunkIndex = INDEX_NONE;
	bPassActive = false;
}

void FDRVoxelTerrainOperationState::StartNextChunk(
	const FDRVoxelTerrainOperationContext& Context)
{
	// 이 함수는 Tick 끝과 StartPass에서 모두 호출되므로 진입 조건을 내부에서 완전히 방어한다.
	// 활성 요청/스캔이 하나라도 있으면 단일 요청 불변식을 깨지 않도록 아무 작업도 하지 않는다.
	if (!Context.bHasAuthority || !Context.bEnableDepositAccumulation || !bPassActive ||
		bHasActiveRequest || bSurfaceScanActive || bFootprintScanActive)
	{
		return;
	}

	if (!IsValid(Context.VoxelWorld) || !Context.VoxelWorld->IsCreated() ||
		Context.TerrainChunks == nullptr || Context.DepositSettings == nullptr)
	{
		Cancel();
		return;
	}

	while (NextChunkOrderIndex < ChunkOrder.Num())
	{
		// 청크 레이아웃이 예기치 않게 바뀐 경우 잘못된 인덱스만 건너뛴다.
		// 정상 경로에서는 Actor가 레이아웃 변경 전에 전체 Job을 취소한다.
		const int32 ChunkIndex = ChunkOrder[NextChunkOrderIndex++];
		if (!Context.TerrainChunks->IsValidIndex(ChunkIndex))
		{
			continue;
		}

		FDRVoxelTerrainChunkBounds& Chunk = (*Context.TerrainChunks)[ChunkIndex];
		FDRVoxelDepositInBoxSettings RequestSettings = *Context.DepositSettings;
		// 같은 전역 설정을 사용하더라도 청크마다 후보 패턴이 반복되지 않도록 Seed를 분리한다.
		RequestSettings.RandomSeed = FMath::Rand();

		FDRVoxelDepositInBoxRequest Request;
		bool bRequestCreated = UDRVoxelTerrainOperationLibrary::MakeDepositInBoxRequest(
			Context.VoxelWorld,
			Chunk.BoxCenter,
			Chunk.BoxExtent,
			RequestSettings,
			Request);
		if (bRequestCreated)
		{
			// 후보 탐색은 청크의 확장 영역을 사용할 수 있지만 실제 쓰기는 Actor 관리 박스 안으로 제한한다.
			// 이 경계가 없으면 가장자리 청크의 풋프린트가 관리 영역 밖 복셀까지 수정할 수 있다.
			bRequestCreated = UDRVoxelTerrainOperationLibrary::ConfigureDepositRequestWriteBounds(
				Request,
				Context.AreaCenter,
				Context.AreaExtent);
		}
		if (!bRequestCreated)
		{
			Chunk.LastProcessedPass = PassNumber;
			UE_LOG(
				LogTemp,
				Verbose,
				TEXT("Failed to create deposit request for chunk (%d, %d)."),
				Chunk.ChunkCoordinate.X,
				Chunk.ChunkCoordinate.Y);
			continue;
		}

		Request.bBlockDepositBelowWorldStatic = Context.bBlockDepositBelowStaticMeshes;
		Request.bTraceComplexWorldStaticOcclusion = Context.bTraceComplexStaticMeshSurfaces;
		Request.SharedWrittenVoxelPositions = &PassWrittenVoxelPositions;
		Request.SharedWrittenColumns = &PassWrittenColumns;

		// 모든 설정과 공유 Set 포인터를 연결한 뒤에만 활성 플래그를 올린다.
		// 이후 비동기 콜백은 별도 콜백 함수가 아니라 Tick의 QueryTraceData 경로에서만 소비된다.
		ActiveChunkIndex = ChunkIndex;
		ActiveRequest = MoveTemp(Request);
		bHasActiveRequest = true;

		if (Context.bDepositOnStaticMeshes)
		{
			// 시작 실패는 치명적이지 않다. SurfaceScan이 비활성인 채로 남으면 일반 복셀 후보만 처리한다.
			BeginSurfaceScan(Context, RequestSettings, Chunk.BoxCenter, Chunk.BoxExtent);
		}
		return;
	}

	// 유효한 청크를 모두 소비했다. 공유 중복 방지 Set은 패스 범위 자료이므로 여기서만 폐기한다.
	bPassActive = false;
	ActiveChunkIndex = INDEX_NONE;
	NextChunkOrderIndex = 0;
	ChunkOrder.Reset();
	PassWrittenVoxelPositions.Reset();
	PassWrittenColumns.Reset();
}

FDRVoxelDepositInBoxRequest* FDRVoxelTerrainOperationState::GetActiveRequest(
	const FDRVoxelTerrainOperationContext& Context)
{
	if (!bHasActiveRequest || !IsValid(Context.VoxelWorld) || !Context.VoxelWorld->IsCreated())
	{
		return nullptr;
	}

	// 월드 교체 후 이전 요청이 살아남아 다른 VoxelWorld에 쓰는 것을 막는다.
	// 요청 자체의 bIsValid까지 함께 검사하므로 호출부는 nullptr만 처리하면 된다.
	return ActiveRequest.bIsValid && ActiveRequest.VoxelWorld.Get() == Context.VoxelWorld
		? &ActiveRequest
		: nullptr;
}

FDRVoxelDepositInBoxRequest* FDRVoxelTerrainOperationState::GetActiveRequest(
	const FDRVoxelTerrainOperationContext& Context,
	EDRVoxelDepositRequestPhase ExpectedPhase)
{
	FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(Context);
	return Request != nullptr && Request->Phase == ExpectedPhase ? Request : nullptr;
}

void FDRVoxelTerrainOperationState::ProcessRequest(
	const FDRVoxelTerrainOperationContext& Context,
	FDRVoxelTerrainOperationTickResult& OutResult)
{
	if (!bHasActiveRequest || bSurfaceScanActive)
	{
		return;
	}

	// 1차 StaticMesh 스캔이 끝나기 전에는 BuildCandidates를 진행하지 않는다.
	// 그래야 늦게 도착한 외부 후보가 이미 끝난 후보 선택 단계에서 누락되지 않는다.
	if (GetActiveRequest(Context) == nullptr)
	{
		Cancel();
		OutResult.bCancelled = true;
		return;
	}

	const FDRDepositTickBudgets TickBudgets = GetDepositTickBudgets(Context.PerformancePreset);
	const int32 ProcessedChunkIndex = ActiveChunkIndex;
	OutResult.bProcessedRequest = true;
	// 지형 조작 라이브러리는 요청 내부 Phase 하나만 예산만큼 처리한다. false는 오류가 아니라
	// 요청이 더 이상 남지 않았다는 뜻이며, 현재 작업에서는 곧 청크 완료를 의미한다.
	const bool bRequestRemains =
		UDRVoxelTerrainOperationLibrary::ProcessDepositInBoxRequestTick(
			ActiveRequest,
			TickBudgets.ScanColumns,
			TickBudgets.VoxelWriteAttempts,
			OutResult.ModifiedVoxelCount,
			OutResult.ScannedColumnCount,
			OutResult.DeltaRecord);
	OutResult.RemainingRequestCount = bRequestRemains ? 1 : 0;
	if (bRequestRemains)
	{
		return;
	}

	// 반환 델타는 OutResult에 이미 복사됐으므로 요청을 즉시 초기화해도 안전하다.
	// Actor는 bChunkCompleted를 보고 같은 Tick 끝에 누적 배치를 확정한다.
	ActiveRequest = FDRVoxelDepositInBoxRequest();
	bHasActiveRequest = false;
	if (Context.TerrainChunks != nullptr && Context.TerrainChunks->IsValidIndex(ProcessedChunkIndex))
	{
		(*Context.TerrainChunks)[ProcessedChunkIndex].LastProcessedPass = PassNumber;
	}
	ActiveChunkIndex = INDEX_NONE;
	OutResult.bChunkCompleted = true;
}

bool FDRVoxelTerrainOperationState::BeginSurfaceScan(
	const FDRVoxelTerrainOperationContext& Context,
	const FDRVoxelDepositInBoxSettings& RequestSettings,
	const FVector& ScanCenter,
	const FVector& ScanExtent)
{
	// 이전 청크의 핸들이 남아 있지 않도록 항상 빈 상태에서 시작한다.
	CancelSurfaceScan();

	UWorld* World = Context.World;
	if (!Context.bHasAuthority || !Context.bDepositOnStaticMeshes ||
		!IsValid(World) || !IsValid(Context.VoxelWorld) || !Context.VoxelWorld->IsCreated() ||
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

	// X/Y 격자의 선형 인덱스만 보관해 큰 영역에서도 FVector 배열을 미리 만들지 않는다.
	// 발행 시점에 좌표와 Jitter를 계산하므로 메모리 사용량은 열 개수에 비례한 int32 배열 하나다.
	const int32 TotalColumnCount = static_cast<int32>(ColumnCountX * ColumnCountY);
	SurfaceTraceColumnOrder.SetNumUninitialized(TotalColumnCount);
	for (int32 Index = 0; Index < TotalColumnCount; ++Index)
	{
		SurfaceTraceColumnOrder[Index] = Index;
	}

	// 복셀 후보 난수와 별도의 salt를 사용해 두 분포가 우연히 같은 패턴으로 맞물리지 않게 한다.
	SurfaceTraceRandomStream.Initialize(RequestSettings.RandomSeed ^ 0x5A17C9E3);
	DRVoxelTerrain::ShuffleArray(SurfaceTraceColumnOrder, SurfaceTraceRandomStream);

	SurfaceScanVoxelWorld = Context.VoxelWorld;
	SurfaceScanCenter = ScanCenter;
	SurfaceScanExtent = AbsExtent;
	SurfaceTraceColumnCountY = static_cast<int32>(ColumnCountY);
	NextSurfaceTraceColumnIndex = 0;
	bSurfaceScanActive = true;
	return true;
}

void FDRVoxelTerrainOperationState::ProcessSurfaceScan(
	const FDRVoxelTerrainOperationContext& Context)
{
	if (!bSurfaceScanActive)
	{
		return;
	}

	FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(
		Context,
		EDRVoxelDepositRequestPhase::BuildCandidates);
	if (!Context.bHasAuthority || !Context.bEnableDepositAccumulation ||
		!Context.bDepositOnStaticMeshes || Request == nullptr ||
		SurfaceScanVoxelWorld.Get() != Context.VoxelWorld)
	{
		CancelSurfaceScan();
		return;
	}

	UWorld* World = Context.World;
	if (!IsValid(World))
	{
		CancelSurfaceScan();
		return;
	}

	const FDRDepositTickBudgets TickBudgets = GetDepositTickBudgets(Context.PerformancePreset);
	const float MinimumSurfaceNormalZ = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(Context.MaxStaticMeshSlopeAngle, 0.f, 90.f)));

	// 먼저 지난 Tick에 발행한 결과를 회수한다. 뒤에서부터 RemoveAtSwap하므로
	// 배열 재배치가 아직 검사하지 않은 원소를 건너뛰지 않는다.
	for (int32 HandleIndex = PendingSurfaceTraceHandles.Num() - 1; HandleIndex >= 0; --HandleIndex)
	{
		const FTraceHandle TraceHandle = PendingSurfaceTraceHandles[HandleIndex];
		FTraceDatum TraceData;
		if (World->QueryTraceData(TraceHandle, TraceData))
		{
			PendingSurfaceTraceHandles.RemoveAtSwap(HandleIndex, 1, EAllowShrinking::No);
			for (const FHitResult& Hit : TraceData.OutHits)
			{
				if (!IsEligibleStaticMeshDepositHit(
					Hit,
					MinimumSurfaceNormalZ,
					Context.RequiredStaticMeshSurfaceTag))
				{
					continue;
				}

				// Single trace라도 필터링 대상 히트가 앞에 있을 수 있어 첫 유효 히트만 채택한다.
				SurfaceHitPositions.Add(Hit.ImpactPoint);
				break;
			}
		}
		else if (!World->IsTraceHandleValid(TraceHandle, false))
		{
			PendingSurfaceTraceHandles.RemoveAtSwap(HandleIndex, 1, EAllowShrinking::No);
		}
	}

	// Pending 제한은 발행 예산의 두 배다. 물리 쿼리가 여러 프레임 지연돼도 적당한 파이프라인을
	// 유지하면서 무제한으로 핸들이 쌓이는 상황을 막는다.
	const int32 TraceBudget = FMath::Max(1, TickBudgets.StaticMeshTraces);
	const int32 PendingTraceLimit = FMath::Max(1, TickBudgets.StaticMeshTraces * 2);
	int32 IssuedTraceCount = 0;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DRStaticMeshDepositSurface),
		Context.bTraceComplexStaticMeshSurfaces);
	if (Context.TraceOwner != nullptr)
	{
		QueryParams.AddIgnoredActor(Context.TraceOwner);
	}

	const FVector BoxMin = SurfaceScanCenter - SurfaceScanExtent;
	const FVector BoxMax = SurfaceScanCenter + SurfaceScanExtent;
	// 규칙적인 격자 샘플이 가는 메시를 반복적으로 놓치는 aliasing을 줄이기 위해
	// 각 셀 안에서만 위치를 흔든다. 박스 가장자리에서는 범위를 잘라 영역 밖으로 나가지 않는다.
	const float JitterRadius =
		Request->DepositSettings.SurfaceSampleSpacing * DRStaticMeshSampleJitterRatio;

	while (IssuedTraceCount < TraceBudget &&
		PendingSurfaceTraceHandles.Num() < PendingTraceLimit &&
		NextSurfaceTraceColumnIndex < SurfaceTraceColumnOrder.Num())
	{
		const int32 LinearIndex = SurfaceTraceColumnOrder[NextSurfaceTraceColumnIndex++];
		const int32 ColumnX = LinearIndex / SurfaceTraceColumnCountY;
		const int32 ColumnY = LinearIndex % SurfaceTraceColumnCountY;
		const float BaseX = BoxMin.X + ColumnX * Request->DepositSettings.SurfaceSampleSpacing;
		const float BaseY = BoxMin.Y + ColumnY * Request->DepositSettings.SurfaceSampleSpacing;
		const float MinJitterX = FMath::Max(-JitterRadius, BoxMin.X - BaseX);
		const float MaxJitterX = FMath::Min(JitterRadius, BoxMax.X - BaseX);
		const float MinJitterY = FMath::Max(-JitterRadius, BoxMin.Y - BaseY);
		const float MaxJitterY = FMath::Min(JitterRadius, BoxMax.Y - BaseY);
		const float SampleX = BaseX + SurfaceTraceRandomStream.FRandRange(MinJitterX, MaxJitterX);
		const float SampleY = BaseY + SurfaceTraceRandomStream.FRandRange(MinJitterY, MaxJitterY);

		const FTraceHandle TraceHandle = World->AsyncLineTraceByObjectType(
			EAsyncTraceType::Single,
			FVector(SampleX, SampleY, BoxMax.Z),
			FVector(SampleX, SampleY, BoxMin.Z),
			ObjectQueryParams,
			QueryParams);
		if (TraceHandle.IsValid())
		{
			PendingSurfaceTraceHandles.Add(TraceHandle);
		}
		++IssuedTraceCount;
	}

	// 모든 열을 발행했고 보류 핸들도 없어야 완료다. 발행 완료만으로 끝내면
	// 마지막 프레임의 트레이스 결과가 외부 후보에 포함되지 않는다.
	if (NextSurfaceTraceColumnIndex >= SurfaceTraceColumnOrder.Num() &&
		PendingSurfaceTraceHandles.Num() == 0)
	{
		FinishSurfaceScan(Context);
	}
}

void FDRVoxelTerrainOperationState::FinishSurfaceScan(
	const FDRVoxelTerrainOperationContext& Context)
{
	FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(
		Context,
		EDRVoxelDepositRequestPhase::BuildCandidates);
	if (!bSurfaceScanActive || Request == nullptr)
	{
		CancelSurfaceScan();
		return;
	}

	// 비동기 완료 순서는 프레임 상황에 따라 달라진다. 좌표순으로 정렬한 뒤 요청에 주입해
	// 동일 Seed와 동일 월드에서는 후보 선택 순서가 가능한 한 결정적으로 유지되게 한다.
	SurfaceHitPositions.Sort([](const FVector& A, const FVector& B)
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

	int32 AddedCandidateCount = 0;
	UDRVoxelTerrainOperationLibrary::AddExternalSurfaceDepositCandidates(
		*Request,
		SurfaceHitPositions,
		AddedCandidateCount);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Static mesh deposit scan completed. HitCount=%d AddedCandidateCount=%d"),
		SurfaceHitPositions.Num(),
		AddedCandidateCount);
	CancelSurfaceScan();
}

void FDRVoxelTerrainOperationState::CancelSurfaceScan()
{
	// 물리 시스템의 비동기 요청 자체를 강제 취소하는 API에 의존하지 않는다.
	// 핸들을 잊고 활성 플래그를 내리면 늦게 완료된 결과는 더 이상 Job에 반영되지 않는다.
	PendingSurfaceTraceHandles.Reset();
	SurfaceHitPositions.Reset();
	SurfaceTraceColumnOrder.Reset();
	SurfaceScanVoxelWorld.Reset();
	SurfaceScanCenter = FVector::ZeroVector;
	SurfaceScanExtent = FVector::ZeroVector;
	SurfaceTraceColumnCountY = 0;
	NextSurfaceTraceColumnIndex = 0;
	bSurfaceScanActive = false;
}

bool FDRVoxelTerrainOperationState::BeginFootprintScan(
	const FDRVoxelTerrainOperationContext& Context)
{
	CancelFootprintScan();

	if (!Context.bHasAuthority || !Context.bDepositOnStaticMeshes)
	{
		return false;
	}

	FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(
		Context,
		EDRVoxelDepositRequestPhase::ResolveFootprints);
	if (Request == nullptr)
	{
		return false;
	}

	if (Request->ExternalCandidatePositions.Num() == 0)
	{
		// 외부 후보가 없더라도 완료 플래그는 반드시 세워야 Resolve 단계가 대기하지 않는다.
		Request->bExternalFootprintsResolved = true;
		return true;
	}

	const int32 Radius = Request->DepositFootprintRadius;
	const float MaximumSlopeTangent = FMath::Tan(FMath::DegreesToRadians(
		FMath::Clamp(Context.MaxStaticMeshSlopeAngle, 0.f, 89.f)));
	TMap<FIntPoint, FFootprintTraceTarget> UniqueTargets;

	// 선택된 각 중심의 원형 범위를 셀 단위 대상으로 펼친다. 여러 중심이 같은 XY를 덮으면
	// 트레이스는 한 번만 발행하고, 가장 강한 퇴적량과 모든 중심이 허용하는 Z 범위의 합집합을 보존한다.
	for (const FIntVector& Center : Request->ExternalCandidatePositions)
	{
		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				float AmountScale = 1.f;
				float AllowedHeightDelta = 1.f;
				if (!DRVoxelTerrain::EvaluateFootprintOffset(
					OffsetX,
					OffsetY,
					Radius,
					DRStaticMeshFootprintEdgeStrength,
					MaximumSlopeTangent,
					AmountScale,
					AllowedHeightDelta))
				{
					continue;
				}

				const int64 TargetX64 = static_cast<int64>(Center.X) + OffsetX;
				const int64 TargetY64 = static_cast<int64>(Center.Y) + OffsetY;
				if (TargetX64 < Request->WriteVoxelMin.X || TargetX64 > Request->WriteVoxelMax.X ||
					TargetY64 < Request->WriteVoxelMin.Y || TargetY64 > Request->WriteVoxelMax.Y)
				{
					continue;
				}

				const float CenterSurfaceZ = static_cast<float>(Center.Z - 1);
				const FIntPoint TargetXY(
					static_cast<int32>(TargetX64),
					static_cast<int32>(TargetY64));
				if (FFootprintTraceTarget* ExistingTarget = UniqueTargets.Find(TargetXY))
				{
					ExistingTarget->AmountScale = FMath::Max(ExistingTarget->AmountScale, AmountScale);
					ExistingTarget->MinLocalSurfaceZ = FMath::Min(
						ExistingTarget->MinLocalSurfaceZ,
						CenterSurfaceZ - AllowedHeightDelta);
					ExistingTarget->MaxLocalSurfaceZ = FMath::Max(
						ExistingTarget->MaxLocalSurfaceZ,
						CenterSurfaceZ + AllowedHeightDelta);
				}
				else
				{
					FFootprintTraceTarget NewTarget;
					NewTarget.VoxelXY = TargetXY;
					NewTarget.AmountScale = AmountScale;
					NewTarget.MinLocalSurfaceZ = CenterSurfaceZ - AllowedHeightDelta;
					NewTarget.MaxLocalSurfaceZ = CenterSurfaceZ + AllowedHeightDelta;
					UniqueTargets.Add(TargetXY, NewTarget);
				}
			}
		}
	}

	// TMap 순회 순서는 안정적이지 않으므로 XY 순으로 정렬해 트레이스 발행 순서와 결과 재현성을 맞춘다.
	UniqueTargets.GenerateValueArray(FootprintTraceTargets);
	FootprintTraceTargets.Sort([](
		const FFootprintTraceTarget& A,
		const FFootprintTraceTarget& B)
	{
		if (A.VoxelXY.X != B.VoxelXY.X)
		{
			return A.VoxelXY.X < B.VoxelXY.X;
		}
		return A.VoxelXY.Y < B.VoxelXY.Y;
	});

	// 1차 스캔의 중심 자료는 유지하되 이전 정밀 결과는 비운다. 완료 플래그가 false인 동안
	// 일반 복셀 해석은 외부 풋프린트를 진행하지 않고 이 비동기 스캔 결과를 기다린다.
	Request->ExternalSupportSurfaceZByVoxel.Reset();
	Request->ExternalResolvedAmountScaleByVoxel.Reset();
	Request->bExternalFootprintsResolved = FootprintTraceTargets.Num() == 0;
	FootprintScanCenter = Context.AreaCenter;
	FootprintScanExtent = FVector(
		FMath::Abs(Context.AreaExtent.X),
		FMath::Abs(Context.AreaExtent.Y),
		FMath::Abs(Context.AreaExtent.Z));
	NextFootprintTraceIndex = 0;
	bFootprintScanActive = FootprintTraceTargets.Num() > 0;
	return true;
}

void FDRVoxelTerrainOperationState::ProcessFootprintScan(
	const FDRVoxelTerrainOperationContext& Context)
{
	if (!bFootprintScanActive)
	{
		// 후보 선택을 마쳐 ResolveFootprints Phase에 들어간 순간에만 2차 스캔을 시작한다.
		// 따라서 비싼 복셀 해상도 트레이스는 실제로 선택되지 않은 1차 후보에는 발행되지 않는다.
		FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(
			Context,
			EDRVoxelDepositRequestPhase::ResolveFootprints);
		if (Request == nullptr || Request->bExternalFootprintsResolved)
		{
			return;
		}

		if (!Context.bDepositOnStaticMeshes)
		{
			DiscardStaticMeshFootprintData(*Request);
			return;
		}

		if (!BeginFootprintScan(Context))
		{
			// 시작 실패 시 완료 플래그를 남겨 요청 전체가 영구 정지하는 것을 방지한다.
			DiscardStaticMeshFootprintData(*Request);
		}
		return;
	}

	if (!Context.bHasAuthority || !Context.bEnableDepositAccumulation)
	{
		CancelFootprintScan();
		return;
	}

	FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(
		Context,
		EDRVoxelDepositRequestPhase::ResolveFootprints);
	if (!Context.bDepositOnStaticMeshes || Request == nullptr)
	{
		if (Request != nullptr)
		{
			DiscardStaticMeshFootprintData(*Request);
		}
		CancelFootprintScan();
		return;
	}

	UWorld* World = Context.World;
	if (!IsValid(World))
	{
		DiscardStaticMeshFootprintData(*Request);
		CancelFootprintScan();
		return;
	}

	const float MinimumSurfaceNormalZ = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(Context.MaxStaticMeshSlopeAngle, 0.f, 90.f)));

	// 각 핸들이 자신의 Target을 들고 있으므로 비동기 완료 순서와 상관없이
	// 정확한 XY, 허용 높이, 퇴적 강도에 결과를 되돌릴 수 있다.
	for (int32 PendingIndex = PendingFootprintTraces.Num() - 1;
		PendingIndex >= 0;
		--PendingIndex)
	{
		const FPendingFootprintTrace PendingTrace = PendingFootprintTraces[PendingIndex];
		FTraceDatum TraceData;
		if (World->QueryTraceData(PendingTrace.Handle, TraceData))
		{
			PendingFootprintTraces.RemoveAtSwap(PendingIndex, 1, EAllowShrinking::No);
			for (const FHitResult& Hit : TraceData.OutHits)
			{
				if (!IsEligibleStaticMeshDepositHit(
					Hit,
					MinimumSurfaceNormalZ,
					Context.RequiredStaticMeshSurfaceTag))
				{
					continue;
				}

				// 월드 Z가 아니라 VoxelWorld 로컬 Z로 비교해야 이동/스케일된 월드에서도
				// 후보 중심이 만든 허용 높이 구간과 같은 좌표계를 사용한다.
				const FVoxelVector LocalSurfacePosition =
					Context.VoxelWorld->GlobalToLocalFloat(Hit.ImpactPoint);
				const float LocalSurfaceZ = static_cast<float>(LocalSurfacePosition.Z);
				if (!FMath::IsFinite(LocalSurfaceZ) ||
					LocalSurfaceZ < PendingTrace.Target.MinLocalSurfaceZ ||
					LocalSurfaceZ > PendingTrace.Target.MaxLocalSurfaceZ)
				{
					continue;
				}

				const float ContactSurfaceZ = LocalSurfaceZ - DRStaticMeshContactInsetVoxels;
				const int64 CandidateZ64 = FMath::FloorToInt64(ContactSurfaceZ) + 1;
				if (CandidateZ64 <= Request->WriteVoxelMin.Z ||
					CandidateZ64 > Request->WriteVoxelMax.Z)
				{
					continue;
				}

				const FIntVector DepositPosition(
					PendingTrace.Target.VoxelXY.X,
					PendingTrace.Target.VoxelXY.Y,
					static_cast<int32>(CandidateZ64));
				// 같은 복셀 위치로 여러 후보가 합쳐진 경우 가장 높은 지지면과 가장 강한 양을 남긴다.
				// 이 규칙으로 낮은 겹침 결과가 이미 찾은 유효 표면을 덮어쓰지 못하게 한다.
				float& StoredSurfaceZ = Request->ExternalSupportSurfaceZByVoxel.FindOrAdd(
					DepositPosition,
					ContactSurfaceZ);
				StoredSurfaceZ = FMath::Max(StoredSurfaceZ, ContactSurfaceZ);
				float& StoredAmountScale = Request->ExternalResolvedAmountScaleByVoxel.FindOrAdd(
					DepositPosition,
					PendingTrace.Target.AmountScale);
				StoredAmountScale = FMath::Max(
					StoredAmountScale,
					PendingTrace.Target.AmountScale);
				break;
			}
		}
		else if (!World->IsTraceHandleValid(PendingTrace.Handle, false))
		{
			PendingFootprintTraces.RemoveAtSwap(PendingIndex, 1, EAllowShrinking::No);
		}
	}

	// 1차 스캔과 같은 예산/보류 한도를 사용해 두 물리 단계의 프레임 비용 특성을 일치시킨다.
	const FDRDepositTickBudgets TickBudgets = GetDepositTickBudgets(Context.PerformancePreset);
	const int32 TraceBudget = FMath::Max(1, TickBudgets.StaticMeshTraces);
	const int32 PendingTraceLimit = FMath::Max(1, TickBudgets.StaticMeshTraces * 2);
	int32 IssuedTraceCount = 0;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DRStaticMeshDepositFootprint),
		Context.bTraceComplexStaticMeshSurfaces);
	if (Context.TraceOwner != nullptr)
	{
		QueryParams.AddIgnoredActor(Context.TraceOwner);
	}

	const float TraceTopZ = FootprintScanCenter.Z + FootprintScanExtent.Z;
	const float TraceBottomZ = FootprintScanCenter.Z - FootprintScanExtent.Z;
	while (IssuedTraceCount < TraceBudget &&
		PendingFootprintTraces.Num() < PendingTraceLimit &&
		NextFootprintTraceIndex < FootprintTraceTargets.Num())
	{
		const FFootprintTraceTarget& Target =
			FootprintTraceTargets[NextFootprintTraceIndex++];
		const FVector TargetWorldPosition = Context.VoxelWorld->LocalToGlobalFloatBP(FVector(
			static_cast<float>(Target.VoxelXY.X),
			static_cast<float>(Target.VoxelXY.Y),
			0.f));
		const FTraceHandle TraceHandle = World->AsyncLineTraceByObjectType(
			EAsyncTraceType::Single,
			FVector(TargetWorldPosition.X, TargetWorldPosition.Y, TraceTopZ),
			FVector(TargetWorldPosition.X, TargetWorldPosition.Y, TraceBottomZ),
			ObjectQueryParams,
			QueryParams);
		if (TraceHandle.IsValid())
		{
			FPendingFootprintTrace& PendingTrace =
				PendingFootprintTraces.AddDefaulted_GetRef();
			PendingTrace.Handle = TraceHandle;
			PendingTrace.Target = Target;
		}
		++IssuedTraceCount;
	}

	// 모든 대상의 발행과 모든 비동기 결과 회수가 끝난 뒤에만 요청의 완료 플래그를 세운다.
	if (NextFootprintTraceIndex >= FootprintTraceTargets.Num() &&
		PendingFootprintTraces.Num() == 0)
	{
		FinishFootprintScan(Context);
	}
}

void FDRVoxelTerrainOperationState::FinishFootprintScan(
	const FDRVoxelTerrainOperationContext& Context)
{
	FDRVoxelDepositInBoxRequest* Request = GetActiveRequest(
		Context,
		EDRVoxelDepositRequestPhase::ResolveFootprints);
	if (bFootprintScanActive && Request != nullptr)
	{
		// 이 플래그가 ResolveFootprints 대기 조건을 해제한다.
		// 유효 표면 수가 0이어도 '완료된 빈 결과'이므로 true여야 한다.
		Request->bExternalFootprintsResolved = true;
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("Static mesh footprint scan completed. TargetCount=%d ValidSurfaceCount=%d"),
			FootprintTraceTargets.Num(),
			Request->ExternalSupportSurfaceZByVoxel.Num());
	}

	CancelFootprintScan();
}

void FDRVoxelTerrainOperationState::CancelFootprintScan()
{
	// 요청의 bExternalFootprintsResolved는 여기서 건드리지 않는다. 정상 완료인지,
	// 외부 후보 폐기인지에 따라 호출부가 먼저 의미 있는 값을 기록한 뒤 런타임 핸들만 비운다.
	PendingFootprintTraces.Reset();
	FootprintTraceTargets.Reset();
	FootprintScanCenter = FVector::ZeroVector;
	FootprintScanExtent = FVector::ZeroVector;
	NextFootprintTraceIndex = 0;
	bFootprintScanActive = false;
}
