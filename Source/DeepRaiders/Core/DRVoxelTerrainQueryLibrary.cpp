#include "DRVoxelTerrainQueryLibrary.h"

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
	constexpr float DRVoxelValueScale = 32767.f;

	// LocalIndex가 int32이므로 서버와 클라이언트 모두 이 구조체 생성에 성공한 박스만 압축 델타로 사용한다.
	struct FDRVoxelBoxDimensions
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		int32 SizeZ = 0;
		int32 SizeXY = 0;
		int32 TotalVoxelCount = 0;
	};

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

	bool TryGetVoxelBoxDimensions(
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax,
		FDRVoxelBoxDimensions& OutDimensions)
	{
		OutDimensions = FDRVoxelBoxDimensions();
		if (VoxelMin.X == MIN_int32 || VoxelMin.Y == MIN_int32 || VoxelMin.Z == MIN_int32 ||
			VoxelMax.X == MAX_int32 || VoxelMax.Y == MAX_int32 || VoxelMax.Z == MAX_int32)
		{
			// MaxExclusive 변환과 UpdateBounds.Extend(1)에서 양쪽 경계를 한 칸 확장할 수 있어야 한다.
			return false;
		}

		const int64 SizeX = static_cast<int64>(VoxelMax.X) - VoxelMin.X + 1;
		const int64 SizeY = static_cast<int64>(VoxelMax.Y) - VoxelMin.Y + 1;
		const int64 SizeZ = static_cast<int64>(VoxelMax.Z) - VoxelMin.Z + 1;
		if (SizeX <= 0 || SizeY <= 0 || SizeZ <= 0 ||
			SizeX > MAX_int32 || SizeY > MAX_int32 || SizeZ > MAX_int32)
		{
			return false;
		}

		const int64 SizeXY = SizeX * SizeY;
		if (SizeXY > MAX_int32)
		{
			return false;
		}

		const int64 TotalVoxelCount = SizeXY * SizeZ;
		if (TotalVoxelCount > MAX_int32)
		{
			return false;
		}

		OutDimensions.SizeX = static_cast<int32>(SizeX);
		OutDimensions.SizeY = static_cast<int32>(SizeY);
		OutDimensions.SizeZ = static_cast<int32>(SizeZ);
		OutDimensions.SizeXY = static_cast<int32>(SizeXY);
		OutDimensions.TotalVoxelCount = static_cast<int32>(TotalVoxelCount);
		return true;
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
			FMath::IsFinite(Settings.SampleStep) &&
			FMath::IsFinite(Settings.DepositAmount) &&
			FMath::IsFinite(Settings.JitterRatio) &&
			FMath::IsFinite(Settings.FootprintEdgeStrength) &&
			FMath::IsFinite(Settings.MinSurfaceDepositChance) &&
			FMath::IsFinite(Settings.MaxSurfaceDepositChance) &&
			FMath::IsFinite(Settings.LowerSurfaceSelectionBias);
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
		// 실제 알고리즘이 음수 반경이나 0 이하 지수를 받지 않도록 요청 생성 시 한 번 정규화한다.
		FDRVoxelDepositInBoxSettings Result = Settings;
		Result.JitterRatio = FMath::Clamp(Result.JitterRatio, 0.f, 1.f);
		Result.DepositPatchRadius = FMath::Max(0, Result.DepositPatchRadius);
		Result.DepositFootprintRadius = FMath::Max(0, Result.DepositFootprintRadius);
		Result.FootprintEdgeStrength = FMath::Clamp(Result.FootprintEdgeStrength, 0.f, 1.f);
		Result.MinSurfaceDepositChance = FMath::Clamp(Result.MinSurfaceDepositChance, 0.f, 1.f);
		Result.MaxSurfaceDepositChance = FMath::Clamp(Result.MaxSurfaceDepositChance, 0.f, 1.f);
		Result.LowerSurfaceSelectionBias = FMath::Max(0.01f, Result.LowerSurfaceSelectionBias);
		return Result;
	}

	void ShuffleVoxels(TArray<FIntVector>& Voxels, FRandomStream& RandomStream)
	{
		for (int32 Index = Voxels.Num() - 1; Index > 0; --Index)
		{
			Voxels.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}

	void ShuffleIndices(TArray<int32>& Indices, FRandomStream& RandomStream)
	{
		for (int32 Index = Indices.Num() - 1; Index > 0; --Index)
		{
			Indices.Swap(Index, RandomStream.RandRange(0, Index));
		}
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

	bool IsInsideBounds(const FDRVoxelDepositInBoxRequest& Request, const FIntVector& Position)
	{
		return
			Position.X >= Request.VoxelMin.X && Position.X <= Request.VoxelMax.X &&
			Position.Y >= Request.VoxelMin.Y && Position.Y <= Request.VoxelMax.Y &&
			Position.Z >= Request.VoxelMin.Z && Position.Z <= Request.VoxelMax.Z;
	}

	int32 GetLocalIndex(const FIntVector& Position, const FIntVector& VoxelMin, const FIntVector& VoxelMax)
	{
		// X가 가장 빠르게 증가하고 그다음 Y, 마지막으로 Z가 증가하는 평탄화 방식이다.
		// 클라이언트의 GetPositionFromLocalIndex가 정확히 역연산하므로 두 함수의 순서를 함께 유지해야 한다.
		const int64 SizeX = static_cast<int64>(VoxelMax.X) - VoxelMin.X + 1;
		const int64 SizeY = static_cast<int64>(VoxelMax.Y) - VoxelMin.Y + 1;

		const int64 LocalIndex =
			(Position.X - VoxelMin.X) +
			static_cast<int64>(Position.Y - VoxelMin.Y) * SizeX +
			static_cast<int64>(Position.Z - VoxelMin.Z) * SizeX * SizeY;
		check(LocalIndex >= 0 && LocalIndex <= MAX_int32);
		return static_cast<int32>(LocalIndex);
	}

	FIntVector GetPositionFromLocalIndex(
		int32 LocalIndex,
		const FIntVector& VoxelMin,
		const FDRVoxelBoxDimensions& Dimensions)
	{
		const int32 LocalZ = LocalIndex / Dimensions.SizeXY;
		const int32 Remainder = LocalIndex % Dimensions.SizeXY;
		const int32 LocalY = Remainder / Dimensions.SizeX;
		const int32 LocalX = Remainder % Dimensions.SizeX;

		return FIntVector(
			VoxelMin.X + LocalX,
			VoxelMin.Y + LocalY,
			VoxelMin.Z + LocalZ);
	}

	bool TryAddDepositCandidate(
		FDRVoxelDepositInBoxRequest& Request,
		FVoxelData& Data,
		const FIntVector& DepositVoxelPosition)
	{
		// 후보는 박스 내부의 비어 있는 복셀이어야 한다. Voxel Plugin 밀도 기준으로
		// Value <= 0은 고체, Value > 0은 빈 공간이므로 퇴적할 위치는 현재 값이 양수여야 한다.
		if (!IsInsideBounds(Request, DepositVoxelPosition))
		{
			return false;
		}

		// 패치가 겹치면 같은 위치가 여러 샘플에서 발견될 수 있다. 현재 배치의 후보와
		// 이 요청에서 이미 쓴 위치를 모두 제외해 중복 퇴적과 불필요한 델타를 막는다.
		if (Request.PendingVoxelPositions.Contains(DepositVoxelPosition) ||
			Request.WrittenVoxelPositions.Contains(DepositVoxelPosition))
		{
			return false;
		}

		const float DepositValue = Data.GetValue(DepositVoxelPosition, 0).ToFloat();
		if (DepositValue <= 0.f)
		{
			return false;
		}

		Request.PendingVoxelPositions.Add(DepositVoxelPosition);
		Request.PendingVoxels.Add(DepositVoxelPosition);
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

	struct FDRDepositPatchSurface
	{
		int32 X = 0;
		int32 Y = 0;
		int32 SurfaceZ = 0;
	};

	void AddPatchDepositCandidates(
		FDRVoxelDepositInBoxRequest& Request,
		FVoxelData& Data,
		int32 CenterX,
		int32 CenterY)
	{
		// 한 샘플만 선택하면 점처럼 뾰족하게 쌓이기 쉬우므로 주변 패치의 표면 높이를 함께 조사한다.
		// 패치 안의 최저/최고 높이를 구한 뒤 낮은 표면일수록 높은 확률로 후보에 포함한다.
		const int32 Radius = Request.DepositSettings.DepositPatchRadius;

		TArray<FDRDepositPatchSurface> PatchSurfaces;
		PatchSurfaces.Reserve(FMath::Square(Radius * 2 + 1));

		int32 MinSurfaceZ = MAX_int32;
		int32 MaxSurfaceZ = MIN_int32;

		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				const int64 TargetX64 = static_cast<int64>(CenterX) + OffsetX;
				const int64 TargetY64 = static_cast<int64>(CenterY) + OffsetY;

				if (TargetX64 < Request.VoxelMin.X || TargetX64 > Request.VoxelMax.X ||
					TargetY64 < Request.VoxelMin.Y || TargetY64 > Request.VoxelMax.Y)
				{
					continue;
				}
				const int32 TargetX = static_cast<int32>(TargetX64);
				const int32 TargetY = static_cast<int32>(TargetY64);

				const int32 TargetSurfaceZ = FindTopSurfaceZ(
					Data,
					TargetX,
					TargetY,
					Request.VoxelMin.Z,
					Request.VoxelMax.Z);

				if (TargetSurfaceZ == MIN_int32)
				{
					continue;
				}

				FDRDepositPatchSurface PatchSurface;
				PatchSurface.X = TargetX;
				PatchSurface.Y = TargetY;
				PatchSurface.SurfaceZ = TargetSurfaceZ;
				PatchSurfaces.Add(PatchSurface);

				MinSurfaceZ = FMath::Min(MinSurfaceZ, TargetSurfaceZ);
				MaxSurfaceZ = FMath::Max(MaxSurfaceZ, TargetSurfaceZ);
			}
		}

		if (PatchSurfaces.Num() == 0)
		{
			return;
		}

		// 설정값의 입력 순서가 뒤집혀도 높은 곳에는 작은 확률, 낮은 곳에는 큰 확률이 적용되도록 정렬한다.
		const float MinChance = FMath::Min(
			Request.DepositSettings.MinSurfaceDepositChance,
			Request.DepositSettings.MaxSurfaceDepositChance);
		const float MaxChance = FMath::Max(
			Request.DepositSettings.MinSurfaceDepositChance,
			Request.DepositSettings.MaxSurfaceDepositChance);
		const float HeightRange = static_cast<float>(MaxSurfaceZ - MinSurfaceZ);

		for (const FDRDepositPatchSurface& PatchSurface : PatchSurfaces)
		{
			// LowerSurfaceAlpha: 최고점=0, 최저점=1이다. 높이 차가 없으면 모든 칸을 낮은 칸으로 취급한다.
			// Bias가 1보다 크면 중간 높이의 Alpha가 작아져 선택이 가장 낮은 지점에 더 집중된다.
			const float LowerSurfaceAlpha = HeightRange > 0.f
				? static_cast<float>(MaxSurfaceZ - PatchSurface.SurfaceZ) / HeightRange
				: 1.f;
			const float BiasedLowerSurfaceAlpha = FMath::Pow(
				FMath::Clamp(LowerSurfaceAlpha, 0.f, 1.f),
				Request.DepositSettings.LowerSurfaceSelectionBias);
			const float DepositChance = FMath::Lerp(MinChance, MaxChance, BiasedLowerSurfaceAlpha);

			if (Request.RandomStream.FRand() > DepositChance)
			{
				continue;
			}

			TryAddDepositCandidate(
				Request,
				Data,
				FIntVector(PatchSurface.X, PatchSurface.Y, PatchSurface.SurfaceZ + 1));
		}
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
		Data.SetValue(Position, FVoxelValue(NewValue));
		Data.SetMaterial(Position, DepositMaterial);

		FDRVoxelCompressedValueDelta Delta;
		Delta.LocalIndex = GetLocalIndex(Position, Request.VoxelMin, Request.VoxelMax);
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
		const float ScaledDepositAmount = Request.DepositSettings.DepositAmount * FMath::Max(0.f, AmountScale);
		if (ScaledDepositAmount <= SMALL_NUMBER)
		{
			return false;
		}

		if (!IsInsideBounds(Request, Position) || Request.WrittenVoxelPositions.Contains(Position))
		{
			return false;
		}

		if (Position.Z <= Request.VoxelMin.Z)
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
				IsInsideBounds(Request, BelowPosition) &&
				!Request.WrittenVoxelPositions.Contains(BelowPosition))
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

	void GatherActiveDepositFootprint(
		FDRVoxelDepositInBoxRequest& Request,
		int32& RemainingVoxelWriteAttempts,
		TArray<FDRDepositWriteAttempt>& OutWriteAttempts)
	{
		// 정사각형을 선형 인덱스로 순회하되 원 반경 밖의 칸은 제외한다.
		// 제외된 칸도 "쓰기 시도" 예산을 소비하므로 복잡한 풋프린트에서도 한 틱의 반복 횟수 상한이 일정하다.
		const int32 Radius = Request.DepositSettings.DepositFootprintRadius;
		const int32 SideLength = Radius * 2 + 1;
		const int32 FootprintPositionCount = SideLength * SideLength;
		const float RadiusAsFloat = static_cast<float>(FMath::Max(1, Radius));
		const float EdgeStrength = Request.DepositSettings.FootprintEdgeStrength;

		while (RemainingVoxelWriteAttempts > 0 &&
			Request.NextFootprintOffsetIndex < FootprintPositionCount)
		{
			const int32 OffsetIndex = Request.NextFootprintOffsetIndex++;
			const int32 OffsetX = OffsetIndex / SideLength - Radius;
			const int32 OffsetY = OffsetIndex % SideLength - Radius;
			const float Distance = FMath::Sqrt(
				static_cast<float>(OffsetX * OffsetX + OffsetY * OffsetY));
			RemainingVoxelWriteAttempts--;

			if (Radius > 0 && Distance > RadiusAsFloat + 0.5f)
			{
				continue;
			}

			// 중심에서 가장자리로 갈수록 AmountScale을 1에서 EdgeStrength까지 선형 감소시킨다.
			const float DistanceAlpha = Radius > 0
				? FMath::Clamp(Distance / RadiusAsFloat, 0.f, 1.f)
				: 0.f;
			const float AmountScale = FMath::Lerp(1.f, EdgeStrength, DistanceAlpha);
			const int64 FootprintX = static_cast<int64>(Request.ActiveFootprintCenter.X) + OffsetX;
			const int64 FootprintY = static_cast<int64>(Request.ActiveFootprintCenter.Y) + OffsetY;
			if (FootprintX < Request.VoxelMin.X || FootprintX > Request.VoxelMax.X ||
				FootprintY < Request.VoxelMin.Y || FootprintY > Request.VoxelMax.Y ||
				Request.ActiveFootprintCenter.Z <= Request.VoxelMin.Z)
			{
				// 예산에는 포함하되 실제로 쓸 수 없는 가장자리 칸은 쓰기 잠금 범위에서 제외한다.
				continue;
			}
			const FIntVector FootprintPosition(
				static_cast<int32>(FootprintX),
				static_cast<int32>(FootprintY),
				Request.ActiveFootprintCenter.Z);

			FDRDepositWriteAttempt& WriteAttempt = OutWriteAttempts.AddDefaulted_GetRef();
			WriteAttempt.Position = FootprintPosition;
			WriteAttempt.AmountScale = AmountScale;
		}

		if (Request.NextFootprintOffsetIndex >= FootprintPositionCount)
		{
			// 현재 중심을 모두 순회했으므로 다음 틱/반복에서 새 PendingVoxels 항목을 선택하게 한다.
			Request.NextFootprintOffsetIndex = 0;
			Request.bHasActiveFootprint = false;
		}
	}

	void BuildDepositCandidatesForCurrentColumn(FDRVoxelDepositInBoxRequest& Request, FVoxelData& Data)
	{
		// 규칙적인 샘플 격자가 지형에 그대로 드러나지 않도록 현재 열 중심을 X/Y 방향으로 흔든다.
		// Clamp로 박스 밖으로 나가는 샘플을 경계에 고정한다.
		constexpr int64 MaxSafeJitterRadius = MAX_int32 / 2;
		const int32 JitterRadius = Request.DepositSettings.bUseJitteredSamples
			? static_cast<int32>(FMath::Min(
				FMath::RoundToInt64(
					static_cast<double>(Request.VoxelSampleStep) *
					Request.DepositSettings.JitterRatio),
				MaxSafeJitterRadius))
			: 0;

		const int32 JitterOffsetX = JitterRadius > 0
			? Request.RandomStream.RandRange(-JitterRadius, JitterRadius)
			: 0;
		const int32 JitterOffsetY = JitterRadius > 0
			? Request.RandomStream.RandRange(-JitterRadius, JitterRadius)
			: 0;
		const int32 SampleX = static_cast<int32>(FMath::Clamp(
			static_cast<int64>(Request.ScanCursor.X) + JitterOffsetX,
			static_cast<int64>(Request.VoxelMin.X),
			static_cast<int64>(Request.VoxelMax.X)));
		const int32 SampleY = static_cast<int32>(FMath::Clamp(
			static_cast<int64>(Request.ScanCursor.Y) + JitterOffsetY,
			static_cast<int64>(Request.VoxelMin.Y),
			static_cast<int64>(Request.VoxelMax.Y)));

		if (Request.DepositSettings.bOnlyTopSurface)
		{
			// 일반적인 눈/퇴적물은 최상단 외부 표면만 필요하므로 주변 패치의 최고 표면을 사용한다.
			AddPatchDepositCandidates(
				Request,
				Data,
				SampleX,
				SampleY);

			return;
		}

		// 동굴 천장이나 내부 빈 공간까지 허용하는 모드에서는 한 열의 모든 고체->빈 공간 경계를 후보로 만든다.
		for (int32 Z = Request.VoxelMax.Z - 1; Z >= Request.VoxelMin.Z; --Z)
		{
			const float CurrentValue = Data.GetValue(FIntVector(SampleX, SampleY, Z), 0).ToFloat();
			const float AboveValue = Data.GetValue(FIntVector(SampleX, SampleY, Z + 1), 0).ToFloat();

			if (CurrentValue <= 0.f && AboveValue > 0.f)
			{
				TryAddDepositCandidate(
					Request,
					Data,
					FIntVector(SampleX, SampleY, Z + 1));
			}
		}
	}

	void FinishCandidateBatch(FDRVoxelDepositInBoxRequest& Request)
	{
		// PendingVoxelPositions는 후보 수집 중 중복 검사에만 필요하다.
		// Apply 단계에서는 배열을 사용하므로 집합 메모리를 먼저 비운다.
		Request.PendingVoxelPositions.Reset();

		if (Request.PendingVoxels.Num() == 0)
		{
			if (IsCandidateBuildFinished(Request))
			{
				Request.Phase = EDRVoxelDepositRequestPhase::Finished;
			}
			return;
		}

		// 낮은 위치 선택 확률은 이미 후보 생성 시 반영됐다. 여기서는 선택된 후보의 적용 순서만 섞어
		// 쓰기 예산이 작은 경우에도 배열 앞쪽 공간부터 보이는 현상을 줄인다.
		ShuffleVoxels(Request.PendingVoxels, Request.RandomStream);
		Request.NextVoxelIndex = 0;
		Request.Phase = EDRVoxelDepositRequestPhase::ApplyVoxels;
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

		// 이번 스캔 배치에서 후보가 하나라도 생기면 즉시 Apply 단계로 전환한다.
		// 적용이 끝난 뒤 아직 스캔할 열이 남아 있으면 다시 BuildCandidates 단계로 돌아온다.
		if (Request.PendingVoxels.Num() > 0)
		{
			FinishCandidateBatch(Request);
		}
		else if (IsCandidateBuildFinished(Request))
		{
			Request.Phase = EDRVoxelDepositRequestPhase::Finished;
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
		OutDeltaRecord.VoxelMin = Request.VoxelMin;
		OutDeltaRecord.VoxelMax = Request.VoxelMax;
		OutDeltaRecord.MaterialIndex = Request.DepositSettings.DepositMaterialIndex;

		FVoxelMaterial DepositMaterial;
		DepositMaterial.SetSingleIndex(Request.DepositSettings.DepositMaterialIndex);

		// 풋프린트를 즉시 쓰지 않고 이번 틱 예산만큼의 쓰기 시도를 먼저 모은다.
		// 이렇게 해야 넓은 풋프린트도 틱 예산을 넘지 않으며, 아래에서 하나의 쓰기 잠금으로 처리할 수 있다.
		TArray<FDRDepositWriteAttempt> WriteAttempts;
		WriteAttempts.Reserve(FMath::Max(1, MaxVoxelWriteAttemptsToProcess));
		int32 RemainingVoxelWriteAttempts = FMath::Max(1, MaxVoxelWriteAttemptsToProcess);
		while (RemainingVoxelWriteAttempts > 0)
		{
			if (!Request.bHasActiveFootprint)
			{
				if (Request.NextVoxelIndex >= Request.PendingVoxels.Num())
				{
					break;
				}

				Request.ActiveFootprintCenter = Request.PendingVoxels[Request.NextVoxelIndex++];
				Request.NextFootprintOffsetIndex = 0;
				Request.bHasActiveFootprint = true;
			}

			GatherActiveDepositFootprint(
				Request,
				RemainingVoxelWriteAttempts,
				WriteAttempts);
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

		if (!Request.bHasActiveFootprint && Request.NextVoxelIndex >= Request.PendingVoxels.Num())
		{
			// 현재 후보 배치를 모두 적용했다. 전체 열 스캔까지 끝났으면 Finished,
			// 아니면 다음 틱에 남은 열을 계속 조사하도록 BuildCandidates로 되돌린다.
			Request.PendingVoxels.Reset();
			Request.PendingVoxelPositions.Reset();
			Request.NextVoxelIndex = 0;
			Request.Phase = IsCandidateBuildFinished(Request)
				? EDRVoxelDepositRequestPhase::Finished
				: EDRVoxelDepositRequestPhase::BuildCandidates;
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
		Settings.SampleStep <= 0.f || Settings.DepositAmount <= 0.f ||
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

	if (static_cast<int64>(OutRequest.VoxelMax.Z) - OutRequest.VoxelMin.Z < 1)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	FDRVoxelBoxDimensions BoxDimensions;
	if (!TryGetVoxelBoxDimensions(OutRequest.VoxelMin, OutRequest.VoxelMax, BoxDimensions))
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	OutRequest.DepositSettings = SanitizeDepositSettings(Settings);
	if (!IsSquareRadiusSupported(OutRequest.DepositSettings.DepositPatchRadius) ||
		!IsSquareRadiusSupported(OutRequest.DepositSettings.DepositFootprintRadius))
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	// 월드 단위 SampleStep을 복셀 단위 정수 간격으로 바꾼다. 최소 1로 제한해 무한 루프를 방지한다.
	const double SampleStepInVoxels =
		static_cast<double>(OutRequest.DepositSettings.SampleStep) /
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
	ShuffleIndices(OutRequest.ScanColumnOrder, OutRequest.RandomStream);
	OutRequest.NextScanColumnIndex = 0;
	OutRequest.NextVoxelIndex = 0;
	OutRequest.Phase = EDRVoxelDepositRequestPhase::BuildCandidates;
	OutRequest.bIsValid = true;
	return true;
}

bool UDRVoxelTerrainQueryLibrary::AddExternalSurfaceDepositCandidates(
	FDRVoxelDepositInBoxRequest& Request,
	const TArray<FVector>& SurfaceWorldPositions,
	int32& OutAddedCandidateCount)
{
	OutAddedCandidateCount = 0;

	AVoxelWorld* RequestVoxelWorld = Request.VoxelWorld.Get();
	if (!Request.bIsValid || Request.Phase != EDRVoxelDepositRequestPhase::BuildCandidates ||
		!IsValid(RequestVoxelWorld) || !RequestVoxelWorld->IsCreated())
	{
		return false;
	}

	const int32 FootprintRadius = Request.DepositSettings.DepositFootprintRadius;
	if (!IsSquareRadiusSupported(FootprintRadius))
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
		if (!IsInsideBounds(Request, CandidatePosition) ||
			CandidatePosition.Z <= Request.VoxelMin.Z ||
			Request.PendingVoxelPositions.Contains(CandidatePosition) ||
			Request.WrittenVoxelPositions.Contains(CandidatePosition))
		{
			continue;
		}

		Request.PendingVoxelPositions.Add(CandidatePosition);
		Request.PendingVoxels.Add(CandidatePosition);
		OutAddedCandidateCount++;

		// 현재 풋프린트가 메시 가장자리 밖으로 크게 번지는 것을 완전히 판정하려면 각 칸마다 별도
		// 트레이스가 필요하다. 여기서는 중심 히트의 수평 표면 높이를 원형 풋프린트에 공유해 쿼리 수를
		// 제한한다. 급경사는 액터의 노멀 필터로 제외하고, 정밀한 가장자리는 SampleStep을 줄여 보완한다.
		const float SurfaceZ = static_cast<float>(LocalZ);
		for (int32 OffsetX = -FootprintRadius; OffsetX <= FootprintRadius; ++OffsetX)
		{
			for (int32 OffsetY = -FootprintRadius; OffsetY <= FootprintRadius; ++OffsetY)
			{
				if (OffsetX * OffsetX + OffsetY * OffsetY >
					FootprintRadius * FootprintRadius + FootprintRadius)
				{
					continue;
				}

				const int64 SupportX64 = CandidateX64 + OffsetX;
				const int64 SupportY64 = CandidateY64 + OffsetY;
				if (SupportX64 < Request.VoxelMin.X || SupportX64 > Request.VoxelMax.X ||
					SupportY64 < Request.VoxelMin.Y || SupportY64 > Request.VoxelMax.Y)
				{
					continue;
				}

				const FIntVector SupportedPosition(
					static_cast<int32>(SupportX64),
					static_cast<int32>(SupportY64),
					CandidatePosition.Z);
				float& StoredSurfaceZ = Request.ExternalSupportSurfaceZByVoxel.FindOrAdd(
					SupportedPosition,
					SurfaceZ);
				StoredSurfaceZ = FMath::Max(StoredSurfaceZ, SurfaceZ);
			}
		}
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
	AVoxelWorld* VoxelWorld = Request.VoxelWorld.Get();

	if (!Request.bIsValid || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		Requests.RemoveAt(0, 1, EAllowShrinking::No);
		OutRemainingRequestCount = Requests.Num();
		return false;
	}

	// 한 호출에서는 현재 Phase 하나만 처리한다. Build에서 Apply로 전환되더라도 실제 쓰기는 다음 Tick 호출에서 시작한다.
	// 읽기 스캔과 쓰기를 같은 호출에 몰지 않아 프레임 비용이 갑자기 커지는 것을 막는다.
	if (Request.Phase == EDRVoxelDepositRequestPhase::BuildCandidates)
	{
		ProcessCandidateBuildTick(
			Request,
			VoxelWorld,
			MaxScanColumnsToProcess,
			OutScannedColumnCount);
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
	FDRVoxelBoxDimensions BoxDimensions;
	if (!TryGetVoxelBoxDimensions(DeltaRecord.VoxelMin, DeltaRecord.VoxelMax, BoxDimensions))
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

		const FIntVector Position = GetPositionFromLocalIndex(
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
