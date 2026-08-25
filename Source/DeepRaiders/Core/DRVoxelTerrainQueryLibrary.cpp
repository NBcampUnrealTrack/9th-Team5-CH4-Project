#include "DRVoxelTerrainQueryLibrary.h"
#include "DRVoxelTerrainDepositUtils.h"

#include "Engine/World.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelMaterial.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelWorld.h"

namespace
{
	// 네트워크 델타에서는 float 밀도 값을 int32로 양자화한다.
	// 32767을 사용하면 [-1, 1] 범위를 충분한 정밀도로 보존하면서 플랫폼 간 직렬화 결과도 일정하게 유지할 수 있다.
	constexpr float DRVoxelValueScale = static_cast<float>(DRVoxelTerrain::QuantizedValueMax);
	// 아래 값들은 결과 형태를 안정적으로 유지하면서 에디터 옵션 수를 줄이기 위한 내부 기본값이다.
	constexpr float DRDepositJitterRatio = 0.4f;
	constexpr float DRDepositFootprintEdgeStrength = 0.55f;
	constexpr float DRDepositLowAreaExponent = 1.5f;
	constexpr float DRDepositMaximumLowAreaWeight = 8.f;
	// 풋프린트가 절벽을 넘어 반대편 표면까지 연결되지 않도록 중심과 각 셀 사이에 허용할 최대 경사다.
	// 완만한 언덕은 따라가되 수직에 가까운 면에서는 눈 덩어리가 끊어지도록 내부 기준으로 고정한다.
	constexpr float DRDepositMaximumFootprintSlopeDegrees = 55.f;

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

	bool IsSquareRadiusSupported(int32 Radius)
	{
		// (2R+1)^2 계산과 TArray 인덱스가 int32 범위를 넘지 않는 최대 한 변 길이다.
		constexpr int64 MaxSupportedSideLength = 46340;
		const int64 SideLength = static_cast<int64>(Radius) * 2 + 1;
		return Radius >= 0 && SideLength <= MaxSupportedSideLength;
	}

	bool AreDepositSettingsFinite(const FDRVoxelDepositInBoxSettings& Settings)
	{
		return
			FMath::IsFinite(Settings.SurfaceSampleSpacing) &&
			FMath::IsFinite(Settings.DepositAmountPerPass) &&
			FMath::IsFinite(Settings.SurfaceCoveragePercentPerPass) &&
			FMath::IsFinite(Settings.DepositSpreadRadius) &&
			FMath::IsFinite(Settings.LowAreaPreference);
	}

	AVoxelWorld* GetUsableRequestVoxelWorld(const FDRVoxelDepositInBoxRequest& Request)
	{
		AVoxelWorld* VoxelWorld = Request.VoxelWorld.Get();
		return Request.bIsValid && IsValid(VoxelWorld) && VoxelWorld->IsCreated()
			? VoxelWorld
			: nullptr;
	}

	int32 QuantizeVoxelValue(float Value)
	{
		return FMath::RoundToInt(FMath::Clamp(Value, -1.f, 1.f) * DRVoxelValueScale);
	}

	float DequantizeVoxelValue(int32 Value)
	{
		return FMath::Clamp(static_cast<float>(Value) / DRVoxelValueScale, -1.f, 1.f);
	}

	bool IsDepositRequestFinished(const FDRVoxelDepositInBoxRequest& Request)
	{
		return Request.Phase == EDRVoxelDepositRequestPhase::Finished;
	}

	FDRVoxelDepositInBoxSettings SanitizeDepositSettings(
		const FDRVoxelDepositInBoxSettings& Settings)
	{
		// Blueprint 메타의 Clamp는 에디터 입력을 돕는 기능일 뿐 C++ 호출까지 보장하지 않는다.
		// 실제 알고리즘이 범위를 벗어난 퍼센트나 음수 퍼짐 반경을 받지 않도록 요청 생성 시 한 번 정규화한다.
		FDRVoxelDepositInBoxSettings Result = Settings;
		Result.SurfaceCoveragePercentPerPass = FMath::Clamp(
			Result.SurfaceCoveragePercentPerPass,
			0.f,
			100.f);
		Result.DepositSpreadRadius = FMath::Max(0.f, Result.DepositSpreadRadius);
		Result.LowAreaPreference = FMath::Clamp(Result.LowAreaPreference, 0.f, 1.f);
		return Result;
	}

	bool IsCandidateBuildFinished(const FDRVoxelDepositInBoxRequest& Request)
	{
		return Request.NextScanColumnIndex >= Request.ScanColumnOrder.Num();
	}

	void SetCurrentScanCursor(FDRVoxelDepositInBoxRequest& Request)
	{
		// ScanColumnOrder는 0..(CountX*CountY-1)의 섞인 인덱스다.
		// Y 열 개수로 나누고 나머지를 구해 다시 2차원 열 좌표로 복원한다.
		const int32 LinearIndex = Request.ScanColumnOrder[Request.NextScanColumnIndex];
		const int32 ColumnX = LinearIndex / Request.ScanColumnCountY;
		const int32 ColumnY = LinearIndex % Request.ScanColumnCountY;
		Request.ScanCursor = FIntPoint(
			Request.VoxelMin.X + ColumnX * Request.VoxelSampleStep,
			Request.VoxelMin.Y + ColumnY * Request.VoxelSampleStep);
	}

	void ExpandModifiedBounds(
		const FIntVector& Position,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax)
	{
		if (!bHasModifiedBounds)
		{
			ModifiedMin = Position;
			ModifiedMax = Position;
			bHasModifiedBounds = true;
			return;
		}

		ModifiedMin.X = FMath::Min(ModifiedMin.X, Position.X);
		ModifiedMin.Y = FMath::Min(ModifiedMin.Y, Position.Y);
		ModifiedMin.Z = FMath::Min(ModifiedMin.Z, Position.Z);

		ModifiedMax.X = FMath::Max(ModifiedMax.X, Position.X);
		ModifiedMax.Y = FMath::Max(ModifiedMax.Y, Position.Y);
		ModifiedMax.Z = FMath::Max(ModifiedMax.Z, Position.Z);
	}

	bool IsInsideCoreBounds(const FDRVoxelDepositInBoxRequest& Request, const FIntVector& Position)
	{
		return
			Position.X >= Request.VoxelMin.X && Position.X <= Request.VoxelMax.X &&
			Position.Y >= Request.VoxelMin.Y && Position.Y <= Request.VoxelMax.Y &&
			Position.Z >= Request.VoxelMin.Z && Position.Z <= Request.VoxelMax.Z;
	}

	bool IsInsideWriteBounds(const FDRVoxelDepositInBoxRequest& Request, const FIntVector& Position)
	{
		return
			Position.X >= Request.WriteVoxelMin.X && Position.X <= Request.WriteVoxelMax.X &&
			Position.Y >= Request.WriteVoxelMin.Y && Position.Y <= Request.WriteVoxelMax.Y &&
			Position.Z >= Request.WriteVoxelMin.Z && Position.Z <= Request.WriteVoxelMax.Z;
	}

	bool WasWrittenInCurrentDepositPass(
		const FDRVoxelDepositInBoxRequest& Request,
		const FIntVector& Position)
	{
		// 요청 내부 집합은 단독 라이브러리 호출을 보호하고, 공유 집합은 서로 다른 청크의
		// 확장 풋프린트가 같은 복셀을 다시 누적하는 것을 막는다.
		return Request.WrittenVoxelPositions.Contains(Position) ||
			(Request.SharedWrittenVoxelPositions != nullptr &&
				Request.SharedWrittenVoxelPositions->Contains(Position));
	}

	bool WasColumnWrittenByPreviousChunk(
		const FDRVoxelDepositInBoxRequest& Request,
		const FIntVector& Position)
	{
		return Request.SharedWrittenColumns != nullptr &&
			Request.SharedWrittenColumns->Contains(FIntPoint(Position.X, Position.Y));
	}

	bool CanAddDepositCandidate(
		const FDRVoxelDepositInBoxRequest& Request,
		const FIntVector& Position)
	{
		// 지터 때문에 인접 샘플이 같은 위치를 찾을 수 있다. 현재 청크의 후보와 이번 패스에서 앞선
		// 청크가 이미 덮은 위치/열을 제외해 경계 중복 누적과 불필요한 델타를 막는다.
		return IsInsideCoreBounds(Request, Position) &&
			!Request.PendingVoxelPositions.Contains(Position) &&
			!WasWrittenInCurrentDepositPass(Request, Position) &&
			!WasColumnWrittenByPreviousChunk(Request, Position);
	}

	void AddDepositCandidate(
		FDRVoxelDepositInBoxRequest& Request,
		const FIntVector& Position)
	{
		Request.PendingVoxelPositions.Add(Position);
		Request.PendingVoxels.Add(Position);
	}

	bool TryAddDepositCandidate(
		FDRVoxelDepositInBoxRequest& Request,
		FVoxelData& Data,
		const FIntVector& DepositVoxelPosition)
	{
		// 후보는 박스 내부의 비어 있는 복셀이어야 한다. Voxel Plugin 밀도 기준으로
		// Value <= 0은 고체, Value > 0은 빈 공간이므로 퇴적할 위치는 현재 값이 양수여야 한다.
		if (!CanAddDepositCandidate(Request, DepositVoxelPosition))
		{
			return false;
		}

		const float DepositValue = Data.GetValue(DepositVoxelPosition, 0).ToFloat();
		if (DepositValue <= 0.f)
		{
			return false;
		}

		AddDepositCandidate(Request, DepositVoxelPosition);
		return true;
	}

	int32 FindTopSurfaceZ(
		FVoxelData& Data,
		int32 X,
		int32 Y,
		int32 MinZ,
		int32 MaxZ)
	{
		// 위에서 아래로 내려오며 "현재 칸은 고체, 바로 위 칸은 빈 공간"인 첫 경계를 찾는다.
		// 반환값은 빈 칸이 아니라 고체 표면의 Z이며 실제 퇴적 후보는 호출부에서 Z + 1을 사용한다.
		float AboveValue = Data.GetValue(FIntVector(X, Y, MaxZ), 0).ToFloat();
		// 관리 박스 상단이 천장/지형 고체를 가로지르면 기존 루프는 그 고체층을 빠져나온 뒤
		// 아래쪽 실내 바닥을 첫 표면으로 오인했다. 상단 고체를 잘린 최상단 표면으로 반환하면
		// Z + 1 후보가 Core 밖에서 거절되어 같은 열의 더 낮은 표면까지 스캔이 침투하지 않는다.
		if (AboveValue <= 0.f)
		{
			return MaxZ;
		}

		for (int32 Z = MaxZ - 1; Z >= MinZ; --Z)
		{
			const float CurrentValue = Data.GetValue(FIntVector(X, Y, Z), 0).ToFloat();

			if (CurrentValue <= 0.f && AboveValue > 0.f)
			{
				return Z;
			}

			AboveValue = CurrentValue;
		}

		return MIN_int32;
	}

	int32 FindTopSurfaceZCached(
		FDRVoxelDepositInBoxRequest& Request,
		FVoxelData& Data,
		int32 X,
		int32 Y)
	{
		const FIntPoint Column(X, Y);
		if (const int32* CachedSurfaceZ = Request.TopSurfaceZByColumn.Find(Column))
		{
			return *CachedSurfaceZ;
		}

		const int32 SurfaceZ = FindTopSurfaceZ(
			Data,
			X,
			Y,
			Request.WriteVoxelMin.Z,
			Request.WriteVoxelMax.Z);
		Request.TopSurfaceZByColumn.Add(Column, SurfaceZ);
		return SurfaceZ;
	}

	bool IsCoveredByWorldStatic(
		const FDRVoxelDepositInBoxRequest& Request,
		const FIntVector& Position)
	{
		if (!Request.bBlockDepositBelowWorldStatic ||
			Request.ExternalCandidatePositions.Contains(Position) ||
			Request.ExternalSupportSurfaceZByVoxel.Contains(Position))
		{
			return false;
		}

		AVoxelWorld* VoxelWorld = Request.VoxelWorld.Get();
		UWorld* World = IsValid(VoxelWorld) ? VoxelWorld->GetWorld() : nullptr;
		if (!IsValid(World) || !FMath::IsFinite(Request.WorldBoxTopZ))
		{
			return false;
		}

		FVector TraceStart = VoxelWorld->LocalToGlobalFloatBP(FVector(
			static_cast<float>(Position.X),
			static_cast<float>(Position.Y),
			static_cast<float>(Position.Z)));
		const float TraceInset = FMath::Max(1.f, FMath::Abs(VoxelWorld->VoxelSize) * 0.1f);
		TraceStart.Z += TraceInset;
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, Request.WorldBoxTopZ + TraceInset);
		if (TraceEnd.Z <= TraceStart.Z + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(DRDepositWorldStaticOcclusion),
			Request.bTraceComplexWorldStaticOcclusion);
		QueryParams.AddIgnoredActor(VoxelWorld);
		return World->LineTraceTestByObjectType(
			TraceStart,
			TraceEnd,
			ObjectQueryParams,
			QueryParams);
	}

	struct FDRWeightedDepositCandidate
	{
		FIntVector Position = FIntVector::ZeroValue;
		float Priority = 0.f;
	};

	void RetainSelectedExternalCandidates(FDRVoxelDepositInBoxRequest& Request)
	{
		// 전체 후보 중 퍼센트 선택을 통과한 고정 메시 중심만 남긴다. 풋프린트의 실제 셀과 높이는
		// 이 시점에 추정하지 않고, 액터가 선택된 중심만 대상으로 정밀 비동기 트레이스를 수행해 채운다.
		TSet<FIntVector> SelectedExternalCandidates;
		for (const FIntVector& SelectedPosition : Request.PendingVoxels)
		{
			if (Request.ExternalCandidatePositions.Contains(SelectedPosition))
			{
				SelectedExternalCandidates.Add(SelectedPosition);
			}
		}

		Request.ExternalCandidatePositions = MoveTemp(SelectedExternalCandidates);
		Request.ExternalSupportSurfaceZByVoxel.Reset();
		Request.ExternalResolvedAmountScaleByVoxel.Reset();
	}

	void SelectDepositCandidates(FDRVoxelDepositInBoxRequest& Request)
	{
		// 청크의 복셀 표면과 고정 메시 표면을 모두 조사한 뒤 정확한 목표 개수를 계산한다.
		// 각 후보를 독립 확률로 판정하지 않으므로 작은 청크에서도 설정한 커버리지 비율이 크게 흔들리지 않는다.
		Request.DetectedSurfaceCount = Request.PendingVoxels.Num();
		const int32 TargetSelectionCount = FMath::Clamp(
			FMath::CeilToInt(
				static_cast<double>(Request.DetectedSurfaceCount) *
				static_cast<double>(Request.DepositSettings.SurfaceCoveragePercentPerPass) /
				100.0),
			0,
			Request.DetectedSurfaceCount);

		if (TargetSelectionCount <= 0)
		{
			Request.PendingVoxels.Reset();
			Request.PendingVoxelPositions.Reset();
			Request.ExternalCandidatePositions.Reset();
			Request.ExternalSupportSurfaceZByVoxel.Reset();
			Request.ExternalResolvedAmountScaleByVoxel.Reset();
			Request.SelectedSurfaceCount = 0;
			Request.Phase = EDRVoxelDepositRequestPhase::Finished;
			return;
		}

		int32 MinCandidateZ = MAX_int32;
		int32 MaxCandidateZ = MIN_int32;
		for (const FIntVector& CandidatePosition : Request.PendingVoxels)
		{
			MinCandidateZ = FMath::Min(MinCandidateZ, CandidatePosition.Z);
			MaxCandidateZ = FMath::Max(MaxCandidateZ, CandidatePosition.Z);
		}

		const float HeightRange = static_cast<float>(MaxCandidateZ - MinCandidateZ);
		TArray<FDRWeightedDepositCandidate> WeightedCandidates;
		WeightedCandidates.Reserve(Request.PendingVoxels.Num());

		for (const FIntVector& CandidatePosition : Request.PendingVoxels)
		{
			// LowerSurfaceAlpha는 최고점에서 0, 최저점에서 1이다. LowAreaPreference가 0이면
			// 모든 후보의 가중치가 1이 되어 순수 랜덤 선택이 되고, 1에 가까울수록 낮은 곳이 유리하다.
			const float LowerSurfaceAlpha = HeightRange > 0.f
				? static_cast<float>(MaxCandidateZ - CandidatePosition.Z) / HeightRange
				: 1.f;
			const float BiasedLowerSurfaceAlpha = FMath::Pow(
				FMath::Clamp(LowerSurfaceAlpha, 0.f, 1.f),
				DRDepositLowAreaExponent);
			const float Weight = FMath::Lerp(
				1.f,
				DRDepositMaximumLowAreaWeight,
				Request.DepositSettings.LowAreaPreference * BiasedLowerSurfaceAlpha);

			// 지수 레이스 방식의 키를 사용하면 가중치를 반영하면서도 중복 없이 정확히 K개를 뽑을 수 있다.
			// 작은 Priority가 먼저 선택되며, 같은 RandomSeed에서는 같은 후보 집합이 재현된다.
			FDRWeightedDepositCandidate& WeightedCandidate = WeightedCandidates.AddDefaulted_GetRef();
			WeightedCandidate.Position = CandidatePosition;
			WeightedCandidate.Priority = -FMath::Loge(
				FMath::Max(Request.RandomStream.FRand(), SMALL_NUMBER)) / Weight;
		}

		WeightedCandidates.Sort([](
			const FDRWeightedDepositCandidate& A,
			const FDRWeightedDepositCandidate& B)
		{
			if (A.Priority != B.Priority)
			{
				return A.Priority < B.Priority;
			}
			if (A.Position.Z != B.Position.Z)
			{
				return A.Position.Z < B.Position.Z;
			}
			if (A.Position.X != B.Position.X)
			{
				return A.Position.X < B.Position.X;
			}
			return A.Position.Y < B.Position.Y;
		});

		Request.PendingVoxels.Reset(TargetSelectionCount);
		for (int32 Index = 0; Index < TargetSelectionCount; ++Index)
		{
			Request.PendingVoxels.Add(WeightedCandidates[Index].Position);
		}

		Request.SelectedSurfaceCount = Request.PendingVoxels.Num();
		RetainSelectedExternalCandidates(Request);
		Request.PendingVoxelPositions.Reset();
		DRVoxelTerrain::ShuffleArray(Request.PendingVoxels, Request.RandomStream);
		Request.ResolvedVoxelPositions.Reset();
		Request.ResolvedAmountScales.Reset();
		Request.ResolvedVoxelIndexByPosition.Reset();
		Request.NextVoxelIndex = 0;
		Request.NextResolveCandidateIndex = 0;
		Request.NextResolveOffsetIndex = 0;
		Request.bVoxelFootprintsResolved = false;
		Request.bExternalFootprintsResolved = Request.ExternalCandidatePositions.Num() == 0;
		Request.bExternalWritesMerged = false;
		Request.Phase = EDRVoxelDepositRequestPhase::ResolveFootprints;
	}

	void RecordDepositVoxelValue(
		FDRVoxelDepositInBoxRequest& Request,
		FVoxelData& Data,
		const FVoxelMaterial& DepositMaterial,
		const FIntVector& Position,
		float NewValue,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax,
		FDRVoxelDepositDeltaRecord& DeltaRecord,
		int32& OutModifiedVoxelCount)
	{
		// 서버가 실제로 기록한 최종값만 델타에 넣는다. 메시 지지층과 눈에 보이는 퇴적층이
		// 같은 함수를 사용하므로 클라이언트와 중도 난입 플레이어도 동일한 밀도장을 복원한다.
		Request.WrittenVoxelPositions.Add(Position);
		if (Request.SharedWrittenVoxelPositions != nullptr)
		{
			Request.SharedWrittenVoxelPositions->Add(Position);
		}
		if (Request.SharedWrittenColumns != nullptr)
		{
			Request.SharedWrittenColumns->Add(FIntPoint(Position.X, Position.Y));
		}
		Data.SetValue(Position, FVoxelValue(NewValue));
		Data.SetMaterial(Position, DepositMaterial);

		FDRVoxelCompressedValueDelta Delta;
		Delta.LocalIndex = DRVoxelTerrain::GetInclusiveVoxelLocalIndex(
			Position,
			Request.WriteVoxelMin,
			Request.WriteVoxelMax);
		Delta.QuantizedValue = QuantizeVoxelValue(NewValue);
		DeltaRecord.Deltas.Add(Delta);

		ExpandModifiedBounds(Position, bHasModifiedBounds, ModifiedMin, ModifiedMax);
		OutModifiedVoxelCount++;
	}

	bool TryWriteDepositVoxel(
		FDRVoxelDepositInBoxRequest& Request,
		FVoxelData& Data,
		const FVoxelMaterial& DepositMaterial,
		const FIntVector& Position,
		float AmountScale,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax,
		FDRVoxelDepositDeltaRecord& DeltaRecord,
		int32& OutModifiedVoxelCount)
	{
		// 풋프린트 중심에서는 AmountScale=1, 가장자리에서는 FootprintEdgeStrength에 가까워진다.
		// 따라서 넓은 면적을 한 번에 선택해도 가장자리가 중심보다 얇게 쌓인다.
		const float ScaledDepositAmount =
			Request.DepositSettings.DepositAmountPerPass * FMath::Max(0.f, AmountScale);
		if (ScaledDepositAmount <= SMALL_NUMBER)
		{
			return false;
		}

		if (!IsInsideWriteBounds(Request, Position) || WasWrittenInCurrentDepositPass(Request, Position))
		{
			return false;
		}

		if (Position.Z <= Request.WriteVoxelMin.Z)
		{
			return false;
		}

		const FIntVector BelowPosition(Position.X, Position.Y, Position.Z - 1);
		const float BelowValue = Data.GetValue(BelowPosition, 0).ToFloat();
		const float* ExternalSurfaceZ = Request.ExternalSupportSurfaceZByVoxel.Find(Position);

		// 일반 복셀 후보는 바로 아래 고체가 사라졌으면 쓰지 않는다. 후보 스캔 뒤 플레이어가 파낸 경우도
		// 여기서 걸러진다. 반면 고정 메시 후보는 밀도장에 아래 고체가 없으므로 별도로 기록한 표면 높이를
		// 지지 정보로 사용하고, 아래에서 메시 내부의 얇은 복셀 지지층을 함께 만든다.
		if (BelowValue > 0.f && ExternalSurfaceZ == nullptr)
		{
			return false;
		}

		// 대상 칸이 이미 고체가 됐다면 더 이상 "표면 위 빈 칸"이 아니므로 건너뛴다.
		// 다른 지형 편집이 후보 스캔과 쓰기 사이에 발생해도 기존 고체를 덮어쓰지 않기 위한 검사다.
		const float CurrentValue = Data.GetValue(Position, 0).ToFloat();
		if (CurrentValue <= 0.f)
		{
			return false;
		}

		float DepositStartValue = CurrentValue;
		if (ExternalSurfaceZ != nullptr)
		{
			// 복셀 중심과 실제 메시 표면 사이의 거리를 초기 밀도값으로 사용한다. 예를 들어 메시가
			// 아래/위 복셀 중심 사이 30% 높이에 있으면 지지층은 -0.3, 위층은 +0.7 부근에서 시작한다.
			// 두 값 사이의 0 등가면이 메시 표면에 놓이므로 빈 공간의 기본값 1에서 바로 빼는 것보다
			// 첫 누적부터 표면에 붙어 있고 높이도 부드럽게 증가한다.
			const float BaseDepositValue = FMath::Clamp(
				static_cast<float>(Position.Z) - *ExternalSurfaceZ,
				0.f,
				1.f);
			DepositStartValue = FMath::Min(DepositStartValue, BaseDepositValue);

			if (BelowValue > 0.f &&
				IsInsideWriteBounds(Request, BelowPosition) &&
				!WasWrittenInCurrentDepositPass(Request, BelowPosition))
			{
				// StaticMesh는 VoxelWorld의 밀도장에 포함되지 않는다. 메시 바로 안쪽 한 칸을 음수로 만들어야
				// 위쪽 퇴적값과 보간되는 등가면이 생긴다. 이 값도 델타에 넣어 굴착 및 중도 난입 재생과 일치시킨다.
				const float BaseSupportValue = FMath::Clamp(
					static_cast<float>(BelowPosition.Z) - *ExternalSurfaceZ,
					-1.f,
					0.f);
				const float NewSupportValue = FMath::Clamp(
					BaseSupportValue - ScaledDepositAmount,
					-1.f,
					1.f);

				RecordDepositVoxelValue(
					Request,
					Data,
					DepositMaterial,
					BelowPosition,
					NewSupportValue,
					bHasModifiedBounds,
					ModifiedMin,
					ModifiedMax,
					DeltaRecord,
					OutModifiedVoxelCount);
			}
		}

		const float NewValue = FMath::Clamp(DepositStartValue - ScaledDepositAmount, -1.f, 1.f);
		if (FMath::IsNearlyEqual(CurrentValue, NewValue))
		{
			return false;
		}

		RecordDepositVoxelValue(
			Request,
			Data,
			DepositMaterial,
			Position,
			NewValue,
			bHasModifiedBounds,
			ModifiedMin,
			ModifiedMax,
			DeltaRecord,
			OutModifiedVoxelCount);
		return true;
	}

	struct FDRDepositWriteAttempt
	{
		FIntVector Position = FIntVector::ZeroValue;
		float AmountScale = 1.f;
	};

	void AddResolvedDepositWrite(
		FDRVoxelDepositInBoxRequest& Request,
		const FIntVector& Position,
		float AmountScale)
	{
		if (!IsInsideWriteBounds(Request, Position) ||
			Position.Z <= Request.WriteVoxelMin.Z ||
			WasWrittenInCurrentDepositPass(Request, Position))
		{
			return;
		}

		// 여러 선택 중심의 원이 겹치면 가장 강한 감쇠값만 갱신한다. 이미 허용된 셀은 다시
		// 물리 트레이스하지 않으므로 중복 제거가 WorldStatic 검사보다 먼저 수행되어야 한다.
		if (int32* ExistingIndex = Request.ResolvedVoxelIndexByPosition.Find(Position))
		{
			Request.ResolvedAmountScales[*ExistingIndex] = FMath::Max(
				Request.ResolvedAmountScales[*ExistingIndex],
				AmountScale);
			return;
		}

		// 중심은 야외여도 원형 풋프린트 가장자리가 처마/천장 아래로 들어갈 수 있으므로
		// 중복 제거가 끝난 최종 셀에만 WorldStatic 차폐 규칙을 적용한다.
		if (IsCoveredByWorldStatic(Request, Position))
		{
			return;
		}

		const int32 NewIndex = Request.ResolvedVoxelPositions.Add(Position);
		Request.ResolvedAmountScales.Add(FMath::Max(0.f, AmountScale));
		Request.ResolvedVoxelIndexByPosition.Add(Position, NewIndex);
	}

	void FinalizeResolvedDepositWrites(FDRVoxelDepositInBoxRequest& Request)
	{
		if (!Request.bVoxelFootprintsResolved || !Request.bExternalFootprintsResolved)
		{
			return;
		}

		if (!Request.bExternalWritesMerged)
		{
			// 고정 메시 정밀 트레이스 결과도 일반 복셀 해석 결과와 같은 배열로 합친다. 이후 쓰기 단계는
			// 표면 종류를 구분하지 않고 처리하되 ExternalSupportSurfaceZByVoxel 존재 여부로 지지층만 판정한다.
			for (const TPair<FIntVector, float>& SurfacePair : Request.ExternalSupportSurfaceZByVoxel)
			{
				const float* AmountScale = Request.ExternalResolvedAmountScaleByVoxel.Find(SurfacePair.Key);
				AddResolvedDepositWrite(Request, SurfacePair.Key, AmountScale != nullptr ? *AmountScale : 1.f);
			}
			Request.bExternalWritesMerged = true;
		}

		// 비동기 트레이스 완료 순서나 맵 순회 순서가 쓰기 진행 방향으로 보이지 않게 두 병렬 배열을 함께 섞는다.
		for (int32 Index = Request.ResolvedVoxelPositions.Num() - 1; Index > 0; --Index)
		{
			const int32 SwapIndex = Request.RandomStream.RandRange(0, Index);
			Request.ResolvedVoxelPositions.Swap(Index, SwapIndex);
			Request.ResolvedAmountScales.Swap(Index, SwapIndex);
		}

		Request.ResolvedVoxelIndexByPosition.Reset();
		Request.NextVoxelIndex = 0;
		Request.Phase = Request.ResolvedVoxelPositions.Num() > 0
			? EDRVoxelDepositRequestPhase::ApplyVoxels
			: EDRVoxelDepositRequestPhase::Finished;
	}

	void ProcessResolveFootprintsTick(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		int32 MaxResolveAttemptsToProcess)
	{
		const int32 ResolveBudget = FMath::Max(1, MaxResolveAttemptsToProcess);
		int32 ProcessedAttemptCount = 0;

		if (!Request.bVoxelFootprintsResolved)
		{
			const int32 Radius = Request.DepositFootprintRadius;
			const int32 SideLength = Radius * 2 + 1;
			const int32 FootprintPositionCount = SideLength * SideLength;
			const float MaximumSlopeTangent = FMath::Tan(FMath::DegreesToRadians(
				DRDepositMaximumFootprintSlopeDegrees));

			FVoxelData& Data = VoxelWorld->GetData();
			const FVoxelIntBox ReadBounds(
				Request.WriteVoxelMin,
				Request.WriteVoxelMax + FIntVector(1));
			FVoxelReadScopeLock Lock(Data, ReadBounds, FUNCTION_FNAME);

			while (ProcessedAttemptCount < ResolveBudget &&
				Request.NextResolveCandidateIndex < Request.PendingVoxels.Num())
			{
				const FIntVector Center = Request.PendingVoxels[Request.NextResolveCandidateIndex];
				if (Request.ExternalCandidatePositions.Contains(Center))
				{
					// 고정 메시 중심은 밀도장에서 표면을 찾을 수 없으므로 액터의 비동기 트레이스 결과를 기다린다.
					Request.NextResolveCandidateIndex++;
					Request.NextResolveOffsetIndex = 0;
					continue;
				}

				const int32 OffsetIndex = Request.NextResolveOffsetIndex++;
				const int32 OffsetX = OffsetIndex / SideLength - Radius;
				const int32 OffsetY = OffsetIndex % SideLength - Radius;
				ProcessedAttemptCount++;

				float AmountScale = 1.f;
				float AllowedHeightDeltaFloat = 1.f;
				if (DRVoxelTerrain::EvaluateFootprintOffset(
					OffsetX,
					OffsetY,
					Radius,
					DRDepositFootprintEdgeStrength,
					MaximumSlopeTangent,
					AmountScale,
					AllowedHeightDeltaFloat))
				{
					const int64 TargetX64 = static_cast<int64>(Center.X) + OffsetX;
					const int64 TargetY64 = static_cast<int64>(Center.Y) + OffsetY;
					if (TargetX64 >= Request.WriteVoxelMin.X && TargetX64 <= Request.WriteVoxelMax.X &&
						TargetY64 >= Request.WriteVoxelMin.Y && TargetY64 <= Request.WriteVoxelMax.Y)
					{
						const int32 CenterSurfaceZ = Center.Z - 1;
						// 중심에서 멀어질수록 허용 높이 차도 비례해서 늘어난다. 검색 범위를 이 값으로 제한하면
						// 절벽 아래나 위의 관계없는 표면을 같은 눈 패치가 이어 붙이는 현상도 함께 막을 수 있다.
						const int32 AllowedHeightDelta = FMath::CeilToInt(AllowedHeightDeltaFloat);
						// 제한된 중심 높이 주변만 훑으면 그 범위보다 높은 천장을 보지 못하고 실내 바닥을 잡는다.
						// 각 열의 전체 Z에서 찾은 진짜 최상단 표면만 허용하고, 중심과의 경사 제한은 그 뒤에 적용한다.
						const int32 SurfaceZ = FindTopSurfaceZCached(
							Request,
							Data,
							static_cast<int32>(TargetX64),
							static_cast<int32>(TargetY64));
						if (SurfaceZ != MIN_int32 &&
							SurfaceZ < Request.WriteVoxelMax.Z &&
							FMath::Abs(SurfaceZ - CenterSurfaceZ) <= AllowedHeightDelta)
						{
							AddResolvedDepositWrite(
								Request,
								FIntVector(
									static_cast<int32>(TargetX64),
									static_cast<int32>(TargetY64),
									SurfaceZ + 1),
								AmountScale);
						}
					}
				}

				if (Request.NextResolveOffsetIndex >= FootprintPositionCount)
				{
					Request.NextResolveCandidateIndex++;
					Request.NextResolveOffsetIndex = 0;
				}
			}

			Request.bVoxelFootprintsResolved =
				Request.NextResolveCandidateIndex >= Request.PendingVoxels.Num();
		}

		FinalizeResolvedDepositWrites(Request);
	}

	void BuildDepositCandidatesForCurrentColumn(FDRVoxelDepositInBoxRequest& Request, FVoxelData& Data)
	{
		// 규칙적인 샘플 격자가 지형에 그대로 드러나지 않도록 현재 열 중심을 X/Y 방향으로 흔든다.
		// 지터 비율은 내부 기본값으로 고정하며 Clamp로 박스 밖으로 나가는 샘플을 경계에 고정한다.
		constexpr int64 MaxSafeJitterRadius = MAX_int32 / 2;
		const int32 JitterRadius = static_cast<int32>(FMath::Min(
			FMath::RoundToInt64(
				static_cast<double>(Request.VoxelSampleStep) * DRDepositJitterRatio),
			MaxSafeJitterRadius));

		// 결과 좌표를 나중에 Clamp하면 바깥쪽으로 뽑힌 여러 값이 경계 한 줄에 겹쳐 격자선이 드러난다.
		// 대신 현재 기준점에서 Core 안으로 움직일 수 있는 오프셋 범위를 먼저 계산해 그 안에서만 뽑는다.
		const int32 MinJitterOffsetX = FMath::Max(
			-JitterRadius,
			Request.VoxelMin.X - Request.ScanCursor.X);
		const int32 MaxJitterOffsetX = FMath::Min(
			JitterRadius,
			Request.VoxelMax.X - Request.ScanCursor.X);
		const int32 MinJitterOffsetY = FMath::Max(
			-JitterRadius,
			Request.VoxelMin.Y - Request.ScanCursor.Y);
		const int32 MaxJitterOffsetY = FMath::Min(
			JitterRadius,
			Request.VoxelMax.Y - Request.ScanCursor.Y);

		const int32 JitterOffsetX = JitterRadius > 0
			? Request.RandomStream.RandRange(MinJitterOffsetX, MaxJitterOffsetX)
			: 0;
		const int32 JitterOffsetY = JitterRadius > 0
			? Request.RandomStream.RandRange(MinJitterOffsetY, MaxJitterOffsetY)
			: 0;
		const int32 SampleX = Request.ScanCursor.X + JitterOffsetX;
		const int32 SampleY = Request.ScanCursor.Y + JitterOffsetY;

		// 퇴적은 외부에서 보이는 최상단 표면에만 쌓는다. 한 샘플 열마다 후보 중심은 최대 하나이므로
		// 표면 샘플 간격과 커버리지 퍼센트가 실제 처리량을 직접 설명할 수 있다.
		const int32 SurfaceZ = FindTopSurfaceZ(
			Data,
			SampleX,
			SampleY,
			Request.VoxelMin.Z,
			Request.VoxelMax.Z);
		if (SurfaceZ != MIN_int32)
		{
			TryAddDepositCandidate(
				Request,
				Data,
				FIntVector(SampleX, SampleY, SurfaceZ + 1));
		}
	}

	void ProcessCandidateBuildTick(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		int32 MaxScanColumnsToProcess,
		int32& OutScannedColumnCount)
	{
		const int32 ScanColumnsToProcess = FMath::Max(1, MaxScanColumnsToProcess);
		FVoxelData& Data = VoxelWorld->GetData();
		const FVoxelIntBox ReadBounds(Request.VoxelMin, Request.VoxelMax + FIntVector(1));
		{
			// 이번 틱에서 스캔할 열 전체를 하나의 읽기 잠금으로 묶는다.
			// 각 샘플마다 GetValue 도구를 호출할 때 생기는 반복 잠금 비용을 줄이고 같은 스냅샷에서 표면을 판정한다.
			FVoxelReadScopeLock Lock(Data, ReadBounds, FUNCTION_FNAME);

			while (OutScannedColumnCount < ScanColumnsToProcess && !IsCandidateBuildFinished(Request))
			{
				SetCurrentScanCursor(Request);
				BuildDepositCandidatesForCurrentColumn(Request, Data);
				Request.NextScanColumnIndex++;
				OutScannedColumnCount++;
			}
		}

		// 정확한 퍼센트를 계산하려면 청크 안에서 발견 가능한 표면의 전체 개수를 알아야 한다.
		// 따라서 중간 배치에서 바로 쓰지 않고 모든 열 스캔이 끝난 시점에 한 번만 선택 단계로 넘어간다.
		// 정렬과 외부 지지 맵 정리는 VoxelData 읽기 잠금을 해제한 뒤 수행해 다른 지형 편집을 오래 막지 않는다.
		if (IsCandidateBuildFinished(Request))
		{
			SelectDepositCandidates(Request);
		}
	}

	void ProcessApplyVoxelsTick(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		int32 MaxVoxelWriteAttemptsToProcess,
		int32& OutModifiedVoxelCount,
		FDRVoxelDepositDeltaRecord& OutDeltaRecord)
	{
		// 레코드에는 "요청 설정"이 아니라 이번 틱에 실제로 바뀐 최종 복셀만 추가된다.
		// Revision은 이 함수 밖의 서버 액터가 변경 사실을 확인한 뒤 부여한다.
		// LocalIndex는 청크 Core가 아니라 경계를 넘어갈 수 있는 Write 범위를 원점으로 압축한다.
		OutDeltaRecord.VoxelMin = Request.WriteVoxelMin;
		OutDeltaRecord.VoxelMax = Request.WriteVoxelMax;
		OutDeltaRecord.MaterialIndex = Request.DepositSettings.DepositMaterialIndex;

		FVoxelMaterial DepositMaterial;
		DepositMaterial.SetSingleIndex(Request.DepositSettings.DepositMaterialIndex);

		// Resolve 단계에서 실제 표면을 따라 확정한 위치 중 이번 틱 예산만큼만 꺼낸다.
		// 높이 검색과 쓰기를 분리했으므로 쓰기 잠금 안에서는 밀도 변경에 필요한 최소 확인만 수행한다.
		TArray<FDRDepositWriteAttempt> WriteAttempts;
		WriteAttempts.Reserve(FMath::Max(1, MaxVoxelWriteAttemptsToProcess));
		const int32 WriteAttemptLimit = FMath::Max(1, MaxVoxelWriteAttemptsToProcess);
		while (WriteAttempts.Num() < WriteAttemptLimit &&
			Request.NextVoxelIndex < Request.ResolvedVoxelPositions.Num())
		{
			const int32 ResolvedIndex = Request.NextVoxelIndex++;
			FDRDepositWriteAttempt& WriteAttempt = WriteAttempts.AddDefaulted_GetRef();
			WriteAttempt.Position = Request.ResolvedVoxelPositions[ResolvedIndex];
			WriteAttempt.AmountScale = Request.ResolvedAmountScales.IsValidIndex(ResolvedIndex)
				? Request.ResolvedAmountScales[ResolvedIndex]
				: 1.f;
		}

		bool bHasModifiedBounds = false;
		FIntVector ModifiedMin = FIntVector::ZeroValue;
		FIntVector ModifiedMax = FIntVector::ZeroValue;

		if (WriteAttempts.Num() > 0)
		{
			FIntVector LockMin = WriteAttempts[0].Position;
			FIntVector LockMax = WriteAttempts[0].Position;
			LockMin.Z--;

			for (const FDRDepositWriteAttempt& WriteAttempt : WriteAttempts)
			{
				LockMin.X = FMath::Min(LockMin.X, WriteAttempt.Position.X);
				LockMin.Y = FMath::Min(LockMin.Y, WriteAttempt.Position.Y);
				LockMin.Z = FMath::Min(LockMin.Z, WriteAttempt.Position.Z - 1);
				LockMax.X = FMath::Max(LockMax.X, WriteAttempt.Position.X);
				LockMax.Y = FMath::Max(LockMax.Y, WriteAttempt.Position.Y);
				LockMax.Z = FMath::Max(LockMax.Z, WriteAttempt.Position.Z);
			}

			FVoxelData& Data = VoxelWorld->GetData();
			const FVoxelIntBox WriteBounds(LockMin, LockMax + FIntVector(1));
			{
				// DataTools의 복셀별 Set 호출 대신 FVoxelData에 직접 기록한다.
				// 모든 쓰기가 끝난 뒤 UpdateBounds를 한 번만 호출해 메시 재생성과 렌더 업데이트 중복을 줄인다.
				FVoxelWriteScopeLock Lock(Data, WriteBounds, FUNCTION_FNAME);
				for (const FDRDepositWriteAttempt& WriteAttempt : WriteAttempts)
				{
					TryWriteDepositVoxel(
						Request,
						Data,
						DepositMaterial,
						WriteAttempt.Position,
						WriteAttempt.AmountScale,
						bHasModifiedBounds,
						ModifiedMin,
						ModifiedMax,
						OutDeltaRecord,
						OutModifiedVoxelCount);
				}
			}

			if (bHasModifiedBounds)
			{
				const FVoxelIntBox UpdateBounds(ModifiedMin, ModifiedMax + FIntVector(1));
				UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, UpdateBounds.Extend(1));
			}
		}

		if (Request.NextVoxelIndex >= Request.ResolvedVoxelPositions.Num())
		{
			// 실제 표면 해석을 통과한 모든 위치를 적용했으므로 요청의 큰 임시 배열과 맵을 정리한다.
			Request.PendingVoxels.Reset();
			Request.PendingVoxelPositions.Reset();
			Request.ResolvedVoxelPositions.Reset();
			Request.ResolvedAmountScales.Reset();
			Request.ResolvedVoxelIndexByPosition.Reset();
			Request.NextVoxelIndex = 0;
			Request.ExternalCandidatePositions.Reset();
			Request.ExternalSupportSurfaceZByVoxel.Reset();
			Request.ExternalResolvedAmountScaleByVoxel.Reset();
			Request.Phase = EDRVoxelDepositRequestPhase::Finished;
		}
	}
}

bool UDRVoxelTerrainQueryLibrary::GetMaterialCountsInBox(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	float SampleStep,
	const TArray<uint8>& TargetMaterialIndices,
	TMap<uint8, int32>& OutMaterialCounts,
	int32& OutTotalCount)
{
	// Out 매개변수는 실패하더라도 이전 호출 결과가 남지 않도록 함수 시작 시 항상 초기화한다.
	OutMaterialCounts.Reset();
	OutTotalCount = 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!FMath::IsFinite(SampleStep) || SampleStep <= 0.f ||
		BoxCenter.ContainsNaN() || BoxExtent.ContainsNaN())
	{
		return false;
	}

	const FVector AbsExtent(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z));
	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	// 배열을 Set으로 바꿔 각 샘플의 머터리얼 필터 검사를 O(1)에 가깝게 처리한다.
	// 빈 Set은 필터를 사용하지 않고 발견한 모든 단일 인덱스를 집계한다는 의미다.
	TSet<uint8> TargetMaterialSet;
	for (const uint8 MaterialIndex : TargetMaterialIndices)
	{
		TargetMaterialSet.Add(MaterialIndex);
	}

	const bool bUseMaterialFilter = TargetMaterialSet.Num() > 0;
	const FVector Min = BoxCenter - AbsExtent;
	const FVector Max = BoxCenter + AbsExtent;

	// 이 함수는 정확한 전체 복셀 개수가 아니라 SampleStep 간격의 표본 통계를 구한다.
	// SampleStep이 작을수록 정확도는 높아지지만 X*Y*Z 반복 횟수가 증가하므로 기본적으로 비활성화돼 있다.
	for (float X = Min.X; X <= Max.X; X += SampleStep)
	{
		for (float Y = Min.Y; Y <= Max.Y; Y += SampleStep)
		{
			for (float Z = Min.Z; Z <= Max.Z; Z += SampleStep)
			{
				const FIntVector SampleVoxelPosition = VoxelWorld->GlobalToLocal(FVector(X, Y, Z));

				float Value = 0.f;
				UVoxelDataTools::GetValue(Value, VoxelWorld, SampleVoxelPosition);
				// Voxel Plugin의 밀도 값이 0 이하인 샘플만 고체로 집계한다.
				if (Value > 0.f)
				{
					continue;
				}

				FVoxelMaterial Material;
				UVoxelDataTools::GetMaterial(Material, VoxelWorld, SampleVoxelPosition);

				const uint8 MaterialIndex = Material.GetSingleIndex();
				if (bUseMaterialFilter && !TargetMaterialSet.Contains(MaterialIndex))
				{
					continue;
				}

				OutMaterialCounts.FindOrAdd(MaterialIndex)++;
				OutTotalCount++;
			}
		}
	}

	return true;
}

bool UDRVoxelTerrainQueryLibrary::IsVoxelUpdateInBox(
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	const FVector& Location,
	float Radius)
{
	if (!FMath::IsFinite(Radius) || Radius <= 0.f ||
		BoxCenter.ContainsNaN() || BoxExtent.ContainsNaN() || Location.ContainsNaN())
	{
		return false;
	}

	const FVector AbsExtent(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z));
	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	// 구 중심에서 박스에 가장 가까운 점까지의 거리가 반지름 이하면 구와 박스가 겹친다.
	// 제곱 거리를 사용해 불필요한 제곱근 계산을 피한다.
	const FBox QueryBox(BoxCenter - AbsExtent, BoxCenter + AbsExtent);
	const FVector ClosestPoint = QueryBox.GetClosestPointTo(Location);

	return FVector::DistSquared(ClosestPoint, Location) <= FMath::Square(Radius);
}

bool UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	const FDRVoxelDepositInBoxSettings& Settings,
	FDRVoxelDepositInBoxRequest& OutRequest)
{
	// 실패 경로에서도 호출자에게 부분적으로 초기화된 요청을 넘기지 않도록 먼저 기본값으로 비운다.
	OutRequest = FDRVoxelDepositInBoxRequest();

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	if (!AreDepositSettingsFinite(Settings) ||
		Settings.SurfaceSampleSpacing <= 0.f || Settings.DepositAmountPerPass <= 0.f ||
		!FMath::IsFinite(VoxelWorld->VoxelSize) || VoxelWorld->VoxelSize <= 0.f ||
		BoxCenter.ContainsNaN() || BoxExtent.ContainsNaN())
	{
		return false;
	}

	const FVector AbsExtent(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z));
	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	OutRequest.VoxelWorld = VoxelWorld;
	// 회전된 VoxelWorld에서도 월드 박스를 빠짐없이 감싸도록 여덟 꼭짓점을 모두 로컬 좌표로 변환한다.
	GetLocalVoxelBoundsForWorldBox(
		VoxelWorld,
		BoxCenter,
		AbsExtent,
		OutRequest.VoxelMin,
		OutRequest.VoxelMax);
	// 단독 라이브러리 요청은 기존 동작을 유지하도록 쓰기 범위를 Core와 같게 시작한다.
	// 청크 액터만 요청 생성 직후 ConfigureDepositRequestWriteBounds로 X/Y 여유 범위를 확장한다.
	OutRequest.WriteVoxelMin = OutRequest.VoxelMin;
	OutRequest.WriteVoxelMax = OutRequest.VoxelMax;
	OutRequest.WorldBoxTopZ = BoxCenter.Z + AbsExtent.Z;

	if (static_cast<int64>(OutRequest.VoxelMax.Z) - OutRequest.VoxelMin.Z < 1)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	DRVoxelTerrain::FInclusiveVoxelBoxDimensions BoxDimensions;
	if (!DRVoxelTerrain::TryGetInclusiveVoxelBoxDimensions(
		OutRequest.VoxelMin,
		OutRequest.VoxelMax,
		BoxDimensions,
		true))
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	OutRequest.DepositSettings = SanitizeDepositSettings(Settings);
	// 월드 단위 퍼짐 반경을 실제 쓰기 단계가 사용할 정수 복셀 반경으로 한 번만 변환한다.
	// 너무 큰 값은 정사각형 순회 개수와 배열 인덱스가 int32 범위를 넘을 수 있으므로 요청을 거절한다.
	const double DepositSpreadRadiusInVoxels =
		static_cast<double>(OutRequest.DepositSettings.DepositSpreadRadius) /
		static_cast<double>(VoxelWorld->VoxelSize);
	if (!FMath::IsFinite(DepositSpreadRadiusInVoxels) ||
		DepositSpreadRadiusInVoxels > MAX_int32)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}
	OutRequest.DepositFootprintRadius = FMath::Max(
		0,
		static_cast<int32>(FMath::RoundToInt64(DepositSpreadRadiusInVoxels)));
	if (!IsSquareRadiusSupported(OutRequest.DepositFootprintRadius))
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	// 월드 단위 표면 샘플 간격을 복셀 단위 정수 간격으로 바꾼다. 최소 1로 제한해 무한 루프를 방지한다.
	const double SampleStepInVoxels =
		static_cast<double>(OutRequest.DepositSettings.SurfaceSampleSpacing) /
		static_cast<double>(VoxelWorld->VoxelSize);
	if (!FMath::IsFinite(SampleStepInVoxels) || SampleStepInVoxels > MAX_int32)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}
	OutRequest.VoxelSampleStep = FMath::Max(
		1,
		static_cast<int32>(FMath::RoundToInt64(SampleStepInVoxels)));
	const int64 ScanColumnCountX =
		(static_cast<int64>(OutRequest.VoxelMax.X) - OutRequest.VoxelMin.X) /
		OutRequest.VoxelSampleStep + 1;
	const int64 ScanColumnCountY =
		(static_cast<int64>(OutRequest.VoxelMax.Y) - OutRequest.VoxelMin.Y) /
		OutRequest.VoxelSampleStep + 1;
	// X/Y 열 개수의 곱은 큰 영역에서 int32를 넘을 수 있으므로 int64로 먼저 검증한다.
	// ScanColumnOrder가 int32 인덱스를 사용하므로 그보다 큰 요청은 생성하지 않는다.
	const int64 TotalScanColumnCount = ScanColumnCountX * ScanColumnCountY;
	if (ScanColumnCountX <= 0 || ScanColumnCountY <= 0 ||
		ScanColumnCountY > MAX_int32 || TotalScanColumnCount > MAX_int32)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}
	OutRequest.ScanColumnCountY = static_cast<int32>(ScanColumnCountY);

	OutRequest.ScanCursor = FIntPoint(OutRequest.VoxelMin.X, OutRequest.VoxelMin.Y);
	OutRequest.RandomStream.Initialize(OutRequest.DepositSettings.RandomSeed);
	OutRequest.ScanColumnOrder.SetNumUninitialized(static_cast<int32>(TotalScanColumnCount));
	for (int32 Index = 0; Index < OutRequest.ScanColumnOrder.Num(); ++Index)
	{
		OutRequest.ScanColumnOrder[Index] = Index;
	}
	// 열 순서는 요청 생성 시 한 번만 섞는다. 같은 RandomSeed라면 같은 순서가 만들어져 결과를 재현할 수 있고,
	// 틱 처리 순서가 X/Y 방향으로 고정되지 않아 영역 한쪽부터 규칙적으로 쌓이는 모양도 줄어든다.
	DRVoxelTerrain::ShuffleArray(OutRequest.ScanColumnOrder, OutRequest.RandomStream);
	OutRequest.NextScanColumnIndex = 0;
	OutRequest.NextVoxelIndex = 0;
	OutRequest.Phase = EDRVoxelDepositRequestPhase::BuildCandidates;
	OutRequest.bIsValid = true;
	return true;
}

bool UDRVoxelTerrainQueryLibrary::ConfigureDepositRequestWriteBounds(
	FDRVoxelDepositInBoxRequest& Request,
	const FVector& AreaCenter,
	const FVector& AreaExtent)
{
	AVoxelWorld* RequestVoxelWorld = GetUsableRequestVoxelWorld(Request);
	if (RequestVoxelWorld == nullptr ||
		Request.Phase != EDRVoxelDepositRequestPhase::BuildCandidates ||
		Request.NextScanColumnIndex != 0 || Request.PendingVoxels.Num() > 0 ||
		AreaCenter.ContainsNaN() || AreaExtent.ContainsNaN())
	{
		return false;
	}

	const FVector AbsAreaExtent(
		FMath::Abs(AreaExtent.X),
		FMath::Abs(AreaExtent.Y),
		FMath::Abs(AreaExtent.Z));
	if (AbsAreaExtent.IsNearlyZero())
	{
		return false;
	}

	FIntVector AreaVoxelMin = FIntVector::ZeroValue;
	FIntVector AreaVoxelMax = FIntVector::ZeroValue;
	GetLocalVoxelBoundsForWorldBox(
		RequestVoxelWorld,
		AreaCenter,
		AbsAreaExtent,
		AreaVoxelMin,
		AreaVoxelMax);

	// 후보 중심은 Core에 그대로 두고 X/Y 쓰기 범위만 풋프린트 반경만큼 확장한다.
	// int64에서 먼저 계산해 음수 방향 뺄셈과 큰 좌표의 오버플로를 막은 뒤 전체 관리 영역으로 자른다.
	const int64 Radius = Request.DepositFootprintRadius;
	const int64 ExpandedMinX = static_cast<int64>(Request.VoxelMin.X) - Radius;
	const int64 ExpandedMinY = static_cast<int64>(Request.VoxelMin.Y) - Radius;
	const int64 ExpandedMaxX = static_cast<int64>(Request.VoxelMax.X) + Radius;
	const int64 ExpandedMaxY = static_cast<int64>(Request.VoxelMax.Y) + Radius;

	const FIntVector NewWriteVoxelMin(
		static_cast<int32>(FMath::Max(static_cast<int64>(AreaVoxelMin.X), ExpandedMinX)),
		static_cast<int32>(FMath::Max(static_cast<int64>(AreaVoxelMin.Y), ExpandedMinY)),
		Request.VoxelMin.Z);
	const FIntVector NewWriteVoxelMax(
		static_cast<int32>(FMath::Min(static_cast<int64>(AreaVoxelMax.X), ExpandedMaxX)),
		static_cast<int32>(FMath::Min(static_cast<int64>(AreaVoxelMax.Y), ExpandedMaxY)),
		Request.VoxelMax.Z);

	// 잘못된 관리 박스 또는 회전 변환 때문에 Core가 Write 범위 밖으로 나오는 요청은 사용하지 않는다.
	if (NewWriteVoxelMin.X > Request.VoxelMin.X || NewWriteVoxelMin.Y > Request.VoxelMin.Y ||
		NewWriteVoxelMax.X < Request.VoxelMax.X || NewWriteVoxelMax.Y < Request.VoxelMax.Y)
	{
		return false;
	}

	// 델타의 LocalIndex는 확장된 Write 범위를 기준으로 계산되므로 전체 복셀 수가 int32에 들어와야 한다.
	DRVoxelTerrain::FInclusiveVoxelBoxDimensions WriteBoxDimensions;
	if (!DRVoxelTerrain::TryGetInclusiveVoxelBoxDimensions(
		NewWriteVoxelMin,
		NewWriteVoxelMax,
		WriteBoxDimensions,
		true))
	{
		return false;
	}

	Request.WriteVoxelMin = NewWriteVoxelMin;
	Request.WriteVoxelMax = NewWriteVoxelMax;
	return true;
}

bool UDRVoxelTerrainQueryLibrary::AddExternalSurfaceDepositCandidates(
	FDRVoxelDepositInBoxRequest& Request,
	const TArray<FVector>& SurfaceWorldPositions,
	int32& OutAddedCandidateCount)
{
	OutAddedCandidateCount = 0;

	AVoxelWorld* RequestVoxelWorld = GetUsableRequestVoxelWorld(Request);
	if (RequestVoxelWorld == nullptr ||
		Request.Phase != EDRVoxelDepositRequestPhase::BuildCandidates)
	{
		return false;
	}

	for (const FVector& SurfaceWorldPosition : SurfaceWorldPositions)
	{
		if (SurfaceWorldPosition.ContainsNaN())
		{
			continue;
		}

		// 트레이스 히트를 소수 로컬 복셀 좌표로 유지해야 메시 표면이 두 복셀 중심 사이 어디에 있는지
		// 보존할 수 있다. Z는 표면 바로 위 정수 칸을 선택하고 X/Y는 가장 가까운 열에 맞춘다.
		const FVoxelVector LocalSurfacePosition =
			RequestVoxelWorld->GlobalToLocalFloat(SurfaceWorldPosition);
		const double LocalX = static_cast<double>(LocalSurfacePosition.X);
		const double LocalY = static_cast<double>(LocalSurfacePosition.Y);
		const double LocalZ = static_cast<double>(LocalSurfacePosition.Z);
		if (!FMath::IsFinite(LocalX) || !FMath::IsFinite(LocalY) || !FMath::IsFinite(LocalZ))
		{
			continue;
		}

		const int64 CandidateX64 = FMath::RoundToInt64(LocalX);
		const int64 CandidateY64 = FMath::RoundToInt64(LocalY);
		const int64 CandidateZ64 = FMath::FloorToInt64(LocalZ) + 1;
		if (CandidateX64 < MIN_int32 || CandidateX64 > MAX_int32 ||
			CandidateY64 < MIN_int32 || CandidateY64 > MAX_int32 ||
			CandidateZ64 < MIN_int32 || CandidateZ64 > MAX_int32)
		{
			continue;
		}

		const FIntVector CandidatePosition(
			static_cast<int32>(CandidateX64),
			static_cast<int32>(CandidateY64),
			static_cast<int32>(CandidateZ64));
		if (CandidatePosition.Z <= Request.VoxelMin.Z ||
			!CanAddDepositCandidate(Request, CandidatePosition))
		{
			continue;
		}

		AddDepositCandidate(Request, CandidatePosition);
		Request.ExternalCandidatePositions.Add(CandidatePosition);
		OutAddedCandidateCount++;
	}

	return true;
}

bool UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
	TArray<FDRVoxelDepositInBoxRequest>& Requests,
	int32 MaxScanColumnsToProcess,
	int32 MaxVoxelWriteAttemptsToProcess,
	int32& OutModifiedVoxelCount,
	int32& OutScannedColumnCount,
	FDRVoxelDepositDeltaRecord& OutDeltaRecord,
	int32& OutRemainingRequestCount)
{
	// 출력은 "이번 호출에서 수행한 일"만 표현한다. 이전 틱의 값이 누적되지 않도록 매번 초기화한다.
	OutModifiedVoxelCount = 0;
	OutScannedColumnCount = 0;
	OutRemainingRequestCount = 0;
	OutDeltaRecord = FDRVoxelDepositDeltaRecord();

	if (Requests.Num() == 0)
	{
		return false;
	}

	// 현재 설계는 FIFO 배열의 첫 요청 하나만 진행한다. 액터는 진행 중 요청이 있으면 새 요청을 만들지 않으므로
	// 보통 배열 크기는 0 또는 1이지만, Blueprint/C++ 호출자가 여러 개를 넣어도 순서대로 처리된다.
	FDRVoxelDepositInBoxRequest& Request = Requests[0];
	AVoxelWorld* VoxelWorld = GetUsableRequestVoxelWorld(Request);

	if (VoxelWorld == nullptr)
	{
		Requests.RemoveAt(0, 1, EAllowShrinking::No);
		OutRemainingRequestCount = Requests.Num();
		return false;
	}

	// 한 호출에서는 현재 Phase 하나만 처리한다. 단계가 바뀌어도 다음 단계는 다음 Tick부터 시작하므로
	// 후보 스캔, 풋프린트 높이 해석, 실제 쓰기 비용이 한 프레임에 겹치지 않는다.
	if (Request.Phase == EDRVoxelDepositRequestPhase::BuildCandidates)
	{
		ProcessCandidateBuildTick(
			Request,
			VoxelWorld,
			MaxScanColumnsToProcess,
			OutScannedColumnCount);
	}
	else if (Request.Phase == EDRVoxelDepositRequestPhase::ResolveFootprints)
	{
		ProcessResolveFootprintsTick(
			Request,
			VoxelWorld,
			MaxVoxelWriteAttemptsToProcess);
	}
	else if (Request.Phase == EDRVoxelDepositRequestPhase::ApplyVoxels)
	{
		ProcessApplyVoxelsTick(
			Request,
			VoxelWorld,
			MaxVoxelWriteAttemptsToProcess,
			OutModifiedVoxelCount,
			OutDeltaRecord);
	}

	// 참조 중인 Request를 제거한 뒤에는 다시 접근하지 않는다. 이후에는 배열 개수만 출력한다.
	if (IsDepositRequestFinished(Request))
	{
		Requests.RemoveAt(0, 1, EAllowShrinking::No);
	}

	OutRemainingRequestCount = Requests.Num();
	// 이 bool은 처리 성공 여부가 아니라 "다음 틱에도 Process를 호출할 작업이 남았는가"를 나타낸다.
	return OutRemainingRequestCount > 0;
}

bool UDRVoxelTerrainQueryLibrary::ApplyDepositDeltaRecord(
	AVoxelWorld* VoxelWorld,
	const FDRVoxelDepositDeltaRecord& DeltaRecord,
	int32& OutAppliedVoxelCount)
{
	OutAppliedVoxelCount = 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || DeltaRecord.Deltas.Num() == 0)
	{
		return false;
	}

	FVoxelMaterial Material;
	Material.SetSingleIndex(DeltaRecord.MaterialIndex);

	// 손상되거나 잘못된 네트워크 데이터로 인덱스 역산이 넘치지 않도록 박스 크기와 전체 개수를 검증한다.
	DRVoxelTerrain::FInclusiveVoxelBoxDimensions BoxDimensions;
	if (!DRVoxelTerrain::TryGetInclusiveVoxelBoxDimensions(
		DeltaRecord.VoxelMin,
		DeltaRecord.VoxelMax,
		BoxDimensions,
		true))
	{
		return false;
	}

	TArray<FIntVector> Positions;
	TArray<float> Values;
	Positions.Reserve(DeltaRecord.Deltas.Num());
	Values.Reserve(DeltaRecord.Deltas.Num());

	// 잘못된 LocalIndex를 먼저 걸러내고 실제 위치와 값을 잠금 전에 복원한다.
	// 쓰기 잠금은 유효한 복셀의 최소 경계에만, 실제 기록 시간 동안만 유지한다.
	bool bHasModifiedBounds = false;
	FIntVector ModifiedMin = FIntVector::ZeroValue;
	FIntVector ModifiedMax = FIntVector::ZeroValue;

	for (const FDRVoxelCompressedValueDelta& Delta : DeltaRecord.Deltas)
	{
		if (Delta.LocalIndex < 0 || Delta.LocalIndex >= BoxDimensions.TotalVoxelCount)
		{
			continue;
		}

		const FIntVector Position = DRVoxelTerrain::GetInclusiveVoxelPosition(
			Delta.LocalIndex,
			DeltaRecord.VoxelMin,
			BoxDimensions);
		Positions.Add(Position);
		Values.Add(DequantizeVoxelValue(Delta.QuantizedValue));
		ExpandModifiedBounds(Position, bHasModifiedBounds, ModifiedMin, ModifiedMax);
	}

	if (!bHasModifiedBounds)
	{
		return false;
	}

	FVoxelData& Data = VoxelWorld->GetData();
	const FVoxelIntBox WriteBounds(ModifiedMin, ModifiedMax + FIntVector(1));
	{
		// 서버와 마찬가지로 유효한 모든 델타를 하나의 쓰기 잠금 안에서 적용한다.
		// 레코드에는 최종 밀도 값이 들어 있으므로 현재 클라이언트 값에 DepositAmount를 다시 빼지 않는다.
		FVoxelWriteScopeLock Lock(Data, WriteBounds, FUNCTION_FNAME);
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Data.SetValue(Positions[Index], FVoxelValue(Values[Index]));
			Data.SetMaterial(Positions[Index], Material);
		}
	}

	// 값과 머터리얼 기록이 끝난 뒤 실제 변경 영역을 한 번만 갱신해 메시를 재생성한다.
	OutAppliedVoxelCount = Positions.Num();
	UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, WriteBounds.Extend(1));
	return true;
}

bool UDRVoxelTerrainQueryLibrary::ApplyDigDeltaRecord(
	AVoxelWorld* VoxelWorld,
	const FDRVoxelDigDeltaRecord& DeltaRecord)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!FMath::IsFinite(DeltaRecord.Radius) || DeltaRecord.Radius <= 0.f ||
		DeltaRecord.Location.ContainsNaN())
	{
		return false;
	}

	// 이 함수는 서버에서 지형을 다시 파는 용도가 아니다. 서버 굴착 델리게이트로 기록된 결과를
	// 일반 클라이언트와 중도 난입 클라이언트의 로컬 VoxelWorld에 동일한 구 형태로 재생한다.
	// 기존 GameState 멀티캐스트가 먼저 같은 구를 적용했더라도 여기서 Revision 순서에 맞춰 다시 실행해야
	// 그보다 앞선 퇴적 델타가 늦게 도착한 경우 최종 지형을 서버의 "퇴적 -> 굴착" 순서로 되돌릴 수 있다.
	UVoxelSphereTools::RemoveSphere(
		VoxelWorld,
		DeltaRecord.Location,
		DeltaRecord.Radius,
		nullptr,
		nullptr,
		true,
		true,
		true);
	return true;
}
