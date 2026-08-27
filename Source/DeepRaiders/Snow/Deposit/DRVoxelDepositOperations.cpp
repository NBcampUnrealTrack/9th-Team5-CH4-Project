#include "DRVoxelDepositOperations.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelIntBox.h"
#include "VoxelMaterial.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"

namespace
{
	// 풋프린트 가장자리의 퇴적량 배율입니다.
	constexpr float DRDepositFootprintEdgeStrength = 0.55f;
	// 메시 경계 오차를 줄이기 위한 접촉면 보정값입니다.
	constexpr float DRStaticMeshContactInsetVoxels = 0.1f;
	// 복셀 풋프린트가 따라갈 수 있는 최대 경사입니다.
	constexpr float DRDepositMaximumFootprintSlopeDegrees = 55.f;
	// 요청당 표면 표본 수의 상한입니다.
	constexpr int32 DRMaximumRandomSurfaceSamples = 4096;
	// 요청당 풋프린트 순회 횟수의 상한입니다.
	constexpr int64 DRMaximumDepositFootprintWorkItems = 4096;

	// 표면 후보를 찾은 방식을 구분합니다.
	enum class EDRDepositSurfaceSource : uint8
	{
		Voxel,
		StaticMesh
	};

	struct FDRSurfaceCandidate
	{
		// 표면 바로 위의 빈 복셀 좌표입니다.
		FIntVector Position = FIntVector::ZeroValue;
		// 후보를 찾은 표면 종류입니다.
		EDRDepositSurfaceSource Source = EDRDepositSurfaceSource::Voxel;
	};

	// 같은 X/Y의 중복 메시 트레이스를 합친 대상입니다.
	struct FDRFootprintTraceTarget
	{
		// 트레이스할 로컬 복셀 X/Y 좌표입니다.
		FIntPoint VoxelXY = FIntPoint::ZeroValue;
		// 겹친 풋프린트의 최대 퇴적량 배율입니다.
		float AmountScale = 1.f;
		// 허용할 로컬 표면 Z 범위입니다.
		float MinLocalSurfaceZ = 0.f;
		float MaxLocalSurfaceZ = 0.f;
	};

	// 한 번의 준비 단계에서만 사용하는 작업 상태입니다.
	struct FDRVoxelDepositBuildContext
	{
		// 검사할 복셀 월드입니다.
		AVoxelWorld* VoxelWorld = nullptr;
		// 표면을 검사할 양 끝 포함 복셀 범위입니다.
		FIntVector CoreVoxelMin = FIntVector::ZeroValue;
		FIntVector CoreVoxelMax = FIntVector::ZeroValue;
		// 풋프린트 쓰기를 허용할 양 끝 포함 복셀 범위입니다.
		FIntVector WriteVoxelMin = FIntVector::ZeroValue;
		FIntVector WriteVoxelMax = FIntVector::ZeroValue;
		// 검증된 퇴적 설정입니다.
		FDRVoxelDepositInBoxSettings Settings;
		// 복셀 단위 풋프린트 반경입니다.
		int32 DepositFootprintRadius = 0;
		// 명령 시드 기반 난수 스트림입니다.
		FRandomStream RandomStream;
		// 선택된 X/Y 표본입니다.
		TArray<FIntPoint> ScanSamplePositions;
		// X/Y 열별 최상단 후보입니다.
		TMap<FIntPoint, FDRSurfaceCandidate> TopCandidateByColumn;
		// 최종 풋프린트 중심입니다.
		TArray<FDRSurfaceCandidate> SelectedCandidates;
		// X/Y 열별 최상단 복셀 표면 캐시입니다.
		TMap<FIntPoint, int32> TopSurfaceZByColumn;
		// 좌표별 최종 쓰기입니다.
		TMap<FIntVector, FDRVoxelDepositWrite> WritesByPosition;
		// 메시 천장 검사에 사용할 관리 영역 상단입니다.
		float WorldBoxTopZ = 0.f;
		// 메시 트레이스의 복잡 충돌 사용 여부입니다.
		bool bTraceComplexWorldStatic = false;
	};

	// 월드 박스의 모서리를 포함하는 로컬 복셀 범위를 계산합니다.
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

	// 양 끝 포함 복셀 범위에 속하는지 확인합니다.
	bool IsInsideBounds(
		const FIntVector& Position,
		const FIntVector& BoundsMin,
		const FIntVector& BoundsMax)
	{
		return Position.X >= BoundsMin.X && Position.X <= BoundsMax.X &&
			Position.Y >= BoundsMin.Y && Position.Y <= BoundsMax.Y &&
			Position.Z >= BoundsMin.Z && Position.Z <= BoundsMax.Z;
	}

	// 양 끝 포함 범위 연산이 int32 안에서 안전한지 확인합니다.
	bool IsSafeInclusiveVoxelBounds(
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax)
	{
		if (VoxelMin.X == MIN_int32 || VoxelMin.Y == MIN_int32 || VoxelMin.Z == MIN_int32 ||
			VoxelMax.X == MAX_int32 || VoxelMax.Y == MAX_int32 || VoxelMax.Z == MAX_int32)
		{
			return false;
		}

		const int64 SizeX = static_cast<int64>(VoxelMax.X) - VoxelMin.X + 1;
		const int64 SizeY = static_cast<int64>(VoxelMax.Y) - VoxelMin.Y + 1;
		const int64 SizeZ = static_cast<int64>(VoxelMax.Z) - VoxelMin.Z + 1;
		return SizeX > 0 && SizeY > 0 && SizeZ > 0 &&
			SizeX <= MAX_int32 && SizeY <= MAX_int32 && SizeZ <= MAX_int32 &&
			SizeX <= MAX_int32 / SizeY && SizeX * SizeY <= MAX_int32 / SizeZ;
	}

	// 네트워크 설정을 작업량 제한 안으로 보정합니다.
	FDRVoxelDepositInBoxSettings SanitizeDepositSettings(
		const FDRVoxelDepositInBoxSettings& Settings)
	{
		FDRVoxelDepositInBoxSettings Result = Settings;
		Result.RandomSurfaceSampleCount = FMath::Clamp(
			Result.RandomSurfaceSampleCount,
			1,
			DRMaximumRandomSurfaceSamples);
		Result.MaxSelectedSurfaceCount = FMath::Clamp(
			Result.MaxSelectedSurfaceCount,
			1,
			Result.RandomSurfaceSampleCount);
		Result.DepositSpreadRadius = FMath::Max(0.f, Result.DepositSpreadRadius);
		return Result;
	}

	// 풋프린트 반경에 맞춰 선택 가능한 중심 수를 제한합니다.
	bool ApplyFootprintWorkBudget(
		int32 Radius,
		FDRVoxelDepositInBoxSettings& InOutSettings)
	{
		const int64 SideLength = static_cast<int64>(Radius) * 2 + 1;
		const int64 WorkItemsPerCenter = SideLength * SideLength;
		if (Radius < 0 || SideLength > 46340 || WorkItemsPerCenter <= 0 ||
			WorkItemsPerCenter > DRMaximumDepositFootprintWorkItems)
		{
			return false;
		}

		InOutSettings.MaxSelectedSurfaceCount = FMath::Clamp(
			InOutSettings.MaxSelectedSurfaceCount,
			1,
			static_cast<int32>(DRMaximumDepositFootprintWorkItems / WorkItemsPerCenter));
		return true;
	}

	// 복셀과 메시가 공유할 무작위 X/Y 표본을 선택합니다.
	bool BuildSamplePositions(
		FDRVoxelDepositBuildContext& Context,
		float VoxelSampleSpacing)
	{
		const double StepInVoxels =
			static_cast<double>(VoxelSampleSpacing) /
			static_cast<double>(Context.VoxelWorld->VoxelSize);
		if (!FMath::IsFinite(StepInVoxels) || StepInVoxels > MAX_int32)
		{
			return false;
		}

		const int32 SampleStep = FMath::Max(
			1,
			static_cast<int32>(FMath::RoundToInt64(StepInVoxels)));
		const int64 ColumnCountX =
			(static_cast<int64>(Context.CoreVoxelMax.X) - Context.CoreVoxelMin.X) /
			SampleStep + 1;
		const int64 ColumnCountY =
			(static_cast<int64>(Context.CoreVoxelMax.Y) - Context.CoreVoxelMin.Y) /
			SampleStep + 1;
		if (ColumnCountX <= 0 || ColumnCountY <= 0 ||
			ColumnCountX > MAX_int32 || ColumnCountY > MAX_int32 ||
			ColumnCountX > MAX_int32 / ColumnCountY)
		{
			return false;
		}

		TArray<int32> ColumnOrder;
		DRVoxelDeposit::BuildRandomUniqueIndices(
			static_cast<int32>(ColumnCountX * ColumnCountY),
			Context.Settings.RandomSurfaceSampleCount,
			Context.RandomStream,
			ColumnOrder);

		// 선형 표본 인덱스를 X/Y 격자 좌표로 변환합니다.
		Context.ScanSamplePositions.Reserve(ColumnOrder.Num());
		for (const int32 LinearIndex : ColumnOrder)
		{
			const int32 ColumnX = LinearIndex / static_cast<int32>(ColumnCountY);
			const int32 ColumnY = LinearIndex % static_cast<int32>(ColumnCountY);
			Context.ScanSamplePositions.Add(FIntPoint(
				static_cast<int32>(static_cast<int64>(Context.CoreVoxelMin.X) +
					static_cast<int64>(ColumnX) * SampleStep),
				static_cast<int32>(static_cast<int64>(Context.CoreVoxelMin.Y) +
					static_cast<int64>(ColumnY) * SampleStep)));
		}
		return true;
	}

	// 외부 명령을 검증된 내부 작업 상태로 변환합니다.
	bool InitializeBuildContext(
		AVoxelWorld* VoxelWorld,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelDepositBuildContext& OutContext)
	{
		OutContext = FDRVoxelDepositBuildContext();
		if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
			Command.ScanCenter.ContainsNaN() || Command.ScanExtent.ContainsNaN() ||
			Command.AreaCenter.ContainsNaN() || Command.AreaExtent.ContainsNaN() ||
			!FMath::IsFinite(VoxelWorld->VoxelSize) || VoxelWorld->VoxelSize <= 0.f ||
			!FMath::IsFinite(Command.Settings.SurfaceSampleSpacing) ||
			!FMath::IsFinite(Command.Settings.DepositAmountPerPass) ||
			!FMath::IsFinite(Command.Settings.DepositSpreadRadius) ||
			Command.Settings.SurfaceSampleSpacing <= 0.f ||
			Command.Settings.DepositAmountPerPass <= 0.f ||
			Command.Settings.RandomSurfaceSampleCount <= 0)
		{
			return false;
		}

		const FVector ScanExtent(
			FMath::Abs(Command.ScanExtent.X),
			FMath::Abs(Command.ScanExtent.Y),
			FMath::Abs(Command.ScanExtent.Z));
		const FVector AreaExtent(
			FMath::Abs(Command.AreaExtent.X),
			FMath::Abs(Command.AreaExtent.Y),
			FMath::Abs(Command.AreaExtent.Z));
		if (ScanExtent.IsNearlyZero() || AreaExtent.IsNearlyZero())
		{
			return false;
		}

		OutContext.VoxelWorld = VoxelWorld;
		OutContext.Settings = SanitizeDepositSettings(Command.Settings);
		// 모든 무작위 선택은 명령 시드 기반 스트림을 공유합니다.
		OutContext.RandomStream.Initialize(OutContext.Settings.RandomSeed);
		OutContext.WorldBoxTopZ = Command.AreaCenter.Z + AreaExtent.Z;
		OutContext.bTraceComplexWorldStatic = Command.bTraceComplexStaticMeshSurfaces;

		GetLocalVoxelBoundsForWorldBox(
			VoxelWorld,
			Command.ScanCenter,
			ScanExtent,
			OutContext.CoreVoxelMin,
			OutContext.CoreVoxelMax);
		// 표면 경계를 찾으려면 Z축에 최소 두 층이 필요합니다.
		if (static_cast<int64>(OutContext.CoreVoxelMax.Z) - OutContext.CoreVoxelMin.Z < 1)
		{
			return false;
		}

		if (!IsSafeInclusiveVoxelBounds(
			OutContext.CoreVoxelMin,
			OutContext.CoreVoxelMax))
		{
			return false;
		}

		// 월드 반경을 정수 복셀 반경으로 변환합니다.
		const double RadiusInVoxels =
			static_cast<double>(OutContext.Settings.DepositSpreadRadius) /
			static_cast<double>(VoxelWorld->VoxelSize);
		if (!FMath::IsFinite(RadiusInVoxels) || RadiusInVoxels > MAX_int32)
		{
			return false;
		}

		OutContext.DepositFootprintRadius = FMath::Max(
			0,
			static_cast<int32>(FMath::RoundToInt64(RadiusInVoxels)));
		if (!ApplyFootprintWorkBudget(
			OutContext.DepositFootprintRadius,
			OutContext.Settings))
		{
			return false;
		}

		// 풋프린트 범위를 관리 영역 안에서만 확장합니다.
		FIntVector AreaVoxelMin = FIntVector::ZeroValue;
		FIntVector AreaVoxelMax = FIntVector::ZeroValue;
		GetLocalVoxelBoundsForWorldBox(
			VoxelWorld,
			Command.AreaCenter,
			AreaExtent,
			AreaVoxelMin,
			AreaVoxelMax);
		const int64 Radius = OutContext.DepositFootprintRadius;
		OutContext.WriteVoxelMin = FIntVector(
			static_cast<int32>(FMath::Max(
				static_cast<int64>(AreaVoxelMin.X),
				static_cast<int64>(OutContext.CoreVoxelMin.X) - Radius)),
			static_cast<int32>(FMath::Max(
				static_cast<int64>(AreaVoxelMin.Y),
				static_cast<int64>(OutContext.CoreVoxelMin.Y) - Radius)),
			OutContext.CoreVoxelMin.Z);
		OutContext.WriteVoxelMax = FIntVector(
			static_cast<int32>(FMath::Min(
				static_cast<int64>(AreaVoxelMax.X),
				static_cast<int64>(OutContext.CoreVoxelMax.X) + Radius)),
			static_cast<int32>(FMath::Min(
				static_cast<int64>(AreaVoxelMax.Y),
				static_cast<int64>(OutContext.CoreVoxelMax.Y) + Radius)),
			OutContext.CoreVoxelMax.Z);
		// 관리 영역이 검사 영역을 포함하지 못하면 요청을 거부합니다.
		if (OutContext.WriteVoxelMin.X > OutContext.CoreVoxelMin.X ||
			OutContext.WriteVoxelMin.Y > OutContext.CoreVoxelMin.Y ||
			OutContext.WriteVoxelMax.X < OutContext.CoreVoxelMax.X ||
			OutContext.WriteVoxelMax.Y < OutContext.CoreVoxelMax.Y)
		{
			return false;
		}

		if (!IsSafeInclusiveVoxelBounds(
			OutContext.WriteVoxelMin,
			OutContext.WriteVoxelMax))
		{
			return false;
		}

		return BuildSamplePositions(
			OutContext,
			OutContext.Settings.SurfaceSampleSpacing);
	}

	// Z축 위에서 아래로 첫 빈 공간-고체 경계를 찾아 고체 Z를 반환합니다.
	int32 FindTopSurfaceZ(
		FVoxelData& Data,
		int32 X,
		int32 Y,
		int32 MinZ,
		int32 MaxZ)
	{
		float AboveValue = Data.GetValue(FIntVector(X, Y, MaxZ), 0).ToFloat();
		// 상단이 고체면 천장으로 간주해 더 아래의 표면을 찾지 않습니다.
		if (AboveValue <= 0.f)
		{
			return MaxZ;
		}

		for (int32 Z = MaxZ - 1; Z >= MinZ; --Z)
		{
			const float CurrentValue = Data.GetValue(FIntVector(X, Y, Z), 0).ToFloat();
			// Voxel Plugin은 밀도값 0 이하를 고체로 취급합니다.
			if (CurrentValue <= 0.f && AboveValue > 0.f)
			{
				return Z;
			}
			AboveValue = CurrentValue;
		}
		return MIN_int32;
	}

	// 표면이 없는 열까지 캐시해 반복 검색을 막습니다.
	int32 FindTopSurfaceZCached(
		FDRVoxelDepositBuildContext& Context,
		FVoxelData& Data,
		int32 X,
		int32 Y)
	{
		const FIntPoint Column(X, Y);
		if (const int32* CachedZ = Context.TopSurfaceZByColumn.Find(Column))
		{
			return *CachedZ;
		}

		const int32 SurfaceZ = FindTopSurfaceZ(
			Data,
			X,
			Y,
			Context.WriteVoxelMin.Z,
			Context.WriteVoxelMax.Z);
		Context.TopSurfaceZByColumn.Add(Column, SurfaceZ);
		return SurfaceZ;
	}

	// 각 X/Y 열에는 가장 높은 후보만 남기며 같은 높이면 메시를 우선합니다.
	void OfferSurfaceCandidate(
		FDRVoxelDepositBuildContext& Context,
		const FDRSurfaceCandidate& Candidate)
	{
		if (!IsInsideBounds(
			Candidate.Position,
			Context.CoreVoxelMin,
			Context.CoreVoxelMax))
		{
			return;
		}

		const FIntPoint Column(Candidate.Position.X, Candidate.Position.Y);
		FDRSurfaceCandidate* Existing = Context.TopCandidateByColumn.Find(Column);
		if (Existing == nullptr || Candidate.Position.Z > Existing->Position.Z ||
			(Candidate.Position.Z == Existing->Position.Z &&
				Candidate.Source == EDRDepositSurfaceSource::StaticMesh &&
				Existing->Source != EDRDepositSurfaceSource::StaticMesh))
		{
			Context.TopCandidateByColumn.Add(Column, Candidate);
		}
	}

	// 트레이스 결과에서 월드 Z가 가장 높은 충돌을 선택합니다.
	const FHitResult* FindTopWorldStaticHit(const TArray<FHitResult>& Hits)
	{
		const FHitResult* TopHit = nullptr;
		for (const FHitResult& Hit : Hits)
		{
			if (!Hit.ImpactPoint.ContainsNaN() &&
				(TopHit == nullptr || Hit.ImpactPoint.Z > TopHit->ImpactPoint.Z))
			{
				TopHit = &Hit;
			}
		}
		return TopHit;
	}

	// 충돌이 퇴적 가능한 고정 메시 표면인지 확인합니다.
	bool IsEligibleStaticMeshDepositHit(
		const FHitResult& Hit,
		float MinimumSurfaceNormalZ,
		FName RequiredSurfaceTag)
	{
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

	// 모든 메시 트레이스에 공통 필터를 적용하고 소유 액터를 제외합니다.
	void MakeStaticTraceParameters(
		AActor* TraceOwner,
		bool bTraceComplex,
		FCollisionObjectQueryParams& OutObjectParams,
		FCollisionQueryParams& OutQueryParams)
	{
		OutObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		OutQueryParams = FCollisionQueryParams(
			SCENE_QUERY_STAT(DRStaticMeshDepositSurface),
			bTraceComplex);
		if (TraceOwner != nullptr)
		{
			OutQueryParams.AddIgnoredActor(TraceOwner);
		}
	}

	// 각 표본 열의 최상단 충돌만 검사해 유효하면 메시 후보로 등록합니다.
	bool CollectStaticMeshCandidates(
		UWorld* World,
		AActor* TraceOwner,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelDepositBuildContext& Context)
	{
		if (!Command.bDepositOnStaticMeshes)
		{
			return true;
		}
		if (!IsValid(World))
		{
			return false;
		}

		FCollisionObjectQueryParams ObjectParams;
		FCollisionQueryParams QueryParams;
		MakeStaticTraceParameters(
			TraceOwner,
			Command.bTraceComplexStaticMeshSurfaces,
			ObjectParams,
			QueryParams);
		const float MinimumNormalZ = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 90.f)));
		const float TraceTopZ = Command.ScanCenter.Z + FMath::Abs(Command.ScanExtent.Z);
		const float TraceBottomZ = Command.ScanCenter.Z - FMath::Abs(Command.ScanExtent.Z);

		for (const FIntPoint& SamplePosition : Context.ScanSamplePositions)
		{
			// 로컬 X/Y를 월드로 변환하고 관리 영역의 전체 Z를 검사합니다.
			const FVector SampleWorldPosition = Context.VoxelWorld->LocalToGlobalFloatBP(FVector(
				static_cast<double>(SamplePosition.X),
				static_cast<double>(SamplePosition.Y),
				0.0));
			TArray<FHitResult> Hits;
			World->LineTraceMultiByObjectType(
				Hits,
				FVector(SampleWorldPosition.X, SampleWorldPosition.Y, TraceTopZ),
				FVector(SampleWorldPosition.X, SampleWorldPosition.Y, TraceBottomZ),
				ObjectParams,
				QueryParams);
			const FHitResult* TopHit = FindTopWorldStaticHit(Hits);
			if (TopHit == nullptr || !IsEligibleStaticMeshDepositHit(
				*TopHit,
				MinimumNormalZ,
				Command.RequiredStaticMeshSurfaceTag))
			{
				continue;
			}

			const FVoxelVector LocalHit =
				Context.VoxelWorld->GlobalToLocalFloat(TopHit->ImpactPoint);
			const double LocalZ = static_cast<double>(LocalHit.Z);
			if (!FMath::IsFinite(LocalZ))
			{
				continue;
			}
			// 충돌 표면 바로 위의 복셀을 퇴적 후보로 사용합니다.
			const int64 CandidateZ64 = FMath::FloorToInt64(LocalZ) + 1;
			if (CandidateZ64 < MIN_int32 || CandidateZ64 > MAX_int32)
			{
				continue;
			}

			FDRSurfaceCandidate Candidate;
			Candidate.Position = FIntVector(
				SamplePosition.X,
				SamplePosition.Y,
				static_cast<int32>(CandidateZ64));
			Candidate.Source = EDRDepositSurfaceSource::StaticMesh;
			OfferSurfaceCandidate(Context, Candidate);
		}
		return true;
	}

	// 공유 X/Y 표본에서 최상단 복셀 표면을 수집합니다.
	void CollectVoxelCandidates(FDRVoxelDepositBuildContext& Context)
	{
		FVoxelData& Data = Context.VoxelWorld->GetData();
		const FVoxelIntBox ReadBounds(
			Context.CoreVoxelMin,
			Context.CoreVoxelMax + FIntVector(1));
		FVoxelReadScopeLock Lock(Data, ReadBounds, FUNCTION_FNAME);
		for (const FIntPoint& SamplePosition : Context.ScanSamplePositions)
		{
			const int32 SurfaceZ = FindTopSurfaceZ(
				Data,
				SamplePosition.X,
				SamplePosition.Y,
				Context.CoreVoxelMin.Z,
				Context.CoreVoxelMax.Z);
			if (SurfaceZ != MIN_int32)
			{
				FDRSurfaceCandidate Candidate;
				Candidate.Position = FIntVector(
					SamplePosition.X,
					SamplePosition.Y,
					SurfaceZ + 1);
				Candidate.Source = EDRDepositSurfaceSource::Voxel;
				if (IsInsideBounds(
					Candidate.Position,
					Context.CoreVoxelMin,
					Context.CoreVoxelMax) &&
					Data.GetValue(Candidate.Position, 0).ToFloat() > 0.f)
				{
					OfferSurfaceCandidate(Context, Candidate);
				}
			}
		}
	}

	// 열별 후보를 최대 풋프린트 중심 수로 줄입니다.
	void SelectCandidates(FDRVoxelDepositBuildContext& Context)
	{
		// 안정 정렬로 같은 Z의 무작위 표본 순서를 보존합니다.
		for (const FIntPoint& SamplePosition : Context.ScanSamplePositions)
		{
			if (const FDRSurfaceCandidate* Candidate =
				Context.TopCandidateByColumn.Find(SamplePosition))
			{
				Context.SelectedCandidates.Add(*Candidate);
			}
		}

		Context.SelectedCandidates.StableSort(
			[](const FDRSurfaceCandidate& A, const FDRSurfaceCandidate& B)
			{
				return A.Position.Z < B.Position.Z;
			});
		if (Context.SelectedCandidates.Num() > Context.Settings.MaxSelectedSurfaceCount)
		{
			Context.SelectedCandidates.SetNum(
				Context.Settings.MaxSelectedSurfaceCount,
				EAllowShrinking::No);
		}
		// 낮은 후보를 고른 뒤 적용 순서를 다시 섞습니다.
		DRVoxelDeposit::ShuffleArray(Context.SelectedCandidates, Context.RandomStream);
	}

	// 메시 지지 쓰기는 제외하고 복셀 쓰기 위의 충돌만 천장으로 처리합니다.
	bool IsCoveredByWorldStatic(
		const FDRVoxelDepositBuildContext& Context,
		const FIntVector& Position)
	{
		UWorld* World = Context.VoxelWorld->GetWorld();
		if (!IsValid(World) || !FMath::IsFinite(Context.WorldBoxTopZ))
		{
			return false;
		}

		FVector TraceStart = Context.VoxelWorld->LocalToGlobalFloatBP(FVector(
			static_cast<float>(Position.X),
			static_cast<float>(Position.Y),
			static_cast<float>(Position.Z)));
		// 표면과 즉시 재충돌하지 않도록 시작점을 띄웁니다.
		const float TraceInset = FMath::Max(
			1.f,
			FMath::Abs(Context.VoxelWorld->VoxelSize) * 0.1f);
		TraceStart.Z += TraceInset;
		const FVector TraceEnd(
			TraceStart.X,
			TraceStart.Y,
			Context.WorldBoxTopZ + TraceInset);
		if (TraceEnd.Z <= TraceStart.Z + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(DRDepositWorldStaticOcclusion),
			Context.bTraceComplexWorldStatic);
		QueryParams.AddIgnoredActor(Context.VoxelWorld);
		return World->LineTraceTestByObjectType(
			TraceStart,
			TraceEnd,
			ObjectParams,
			QueryParams);
	}

	// 같은 좌표의 쓰기는 가장 강한 퇴적량으로 병합합니다.
	void AddResolvedWrite(
		FDRVoxelDepositBuildContext& Context,
		const FIntVector& Position,
		float AmountScale,
		const float* StaticMeshSurfaceZ = nullptr)
	{
		if (!IsInsideBounds(Position, Context.WriteVoxelMin, Context.WriteVoxelMax) ||
			Position.Z <= Context.WriteVoxelMin.Z)
		{
			return;
		}

		FDRVoxelDepositWrite* Existing = Context.WritesByPosition.Find(Position);
		// 메시 지지 쓰기를 우선하고 새 복셀 쓰기만 천장을 검사합니다.
		if (Existing == nullptr && StaticMeshSurfaceZ == nullptr &&
			IsCoveredByWorldStatic(Context, Position))
		{
			return;
		}

		FDRVoxelDepositWrite* Write = Existing;
		if (Write == nullptr)
		{
			FDRVoxelDepositWrite NewWrite;
			NewWrite.Position = Position;
			NewWrite.AmountScale = FMath::Max(0.f, AmountScale);
			Context.WritesByPosition.Add(Position, NewWrite);
			Write = Context.WritesByPosition.Find(Position);
		}
		else
		{
			Write->AmountScale = FMath::Max(
				Write->AmountScale,
				FMath::Max(0.f, AmountScale));
		}

		// 겹친 메시 지지면 중 가장 높은 표면을 사용합니다.
		if (StaticMeshSurfaceZ != nullptr)
		{
			const bool bAlreadyHadStaticMeshSupport = Write->bHasStaticMeshSupport;
			Write->bHasStaticMeshSupport = true;
			Write->StaticMeshSurfaceZ = bAlreadyHadStaticMeshSupport
				? FMath::Max(Write->StaticMeshSurfaceZ, *StaticMeshSurfaceZ)
				: *StaticMeshSurfaceZ;
		}
	}

	// 공통 원형 풋프린트를 순회하고 표면별 처리를 콜백에 위임합니다.
	template<typename CallbackType>
	void ForEachFootprintCell(
		const FDRVoxelDepositBuildContext& Context,
		const FDRSurfaceCandidate& Candidate,
		float MaximumSlopeTangent,
		CallbackType&& Callback)
	{
		const int32 Radius = Context.DepositFootprintRadius;
		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				float AmountScale = 1.f;
				float AllowedHeightDelta = 1.f;
				if (!DRVoxelDeposit::EvaluateFootprintOffset(
					OffsetX,
					OffsetY,
					Radius,
					DRDepositFootprintEdgeStrength,
					MaximumSlopeTangent,
					AmountScale,
					AllowedHeightDelta))
				{
					continue;
				}

				const int64 TargetX = static_cast<int64>(Candidate.Position.X) + OffsetX;
				const int64 TargetY = static_cast<int64>(Candidate.Position.Y) + OffsetY;
				if (TargetX < Context.WriteVoxelMin.X || TargetX > Context.WriteVoxelMax.X ||
					TargetY < Context.WriteVoxelMin.Y || TargetY > Context.WriteVoxelMax.Y)
				{
					continue;
				}

				Callback(
					FIntPoint(static_cast<int32>(TargetX), static_cast<int32>(TargetY)),
					AmountScale,
					AllowedHeightDelta);
			}
		}
	}

	// 복셀 중심 주변의 허용 경사 안에서 풋프린트 쓰기를 만듭니다.
	void ResolveVoxelFootprints(FDRVoxelDepositBuildContext& Context)
	{
		const float MaximumSlopeTangent = FMath::Tan(FMath::DegreesToRadians(
			DRDepositMaximumFootprintSlopeDegrees));
		FVoxelData& Data = Context.VoxelWorld->GetData();
		const FVoxelIntBox ReadBounds(
			Context.WriteVoxelMin,
			Context.WriteVoxelMax + FIntVector(1));
		FVoxelReadScopeLock Lock(Data, ReadBounds, FUNCTION_FNAME);

		for (const FDRSurfaceCandidate& Candidate : Context.SelectedCandidates)
		{
			if (Candidate.Source != EDRDepositSurfaceSource::Voxel)
			{
				continue;
			}

			ForEachFootprintCell(
				Context,
				Candidate,
				MaximumSlopeTangent,
				[&](const FIntPoint& TargetXY, float AmountScale, float AllowedHeightDelta)
				{
					const int32 SurfaceZ = FindTopSurfaceZCached(
						Context,
						Data,
						TargetXY.X,
						TargetXY.Y);
					if (SurfaceZ != MIN_int32 && SurfaceZ < Context.WriteVoxelMax.Z &&
						FMath::Abs(SurfaceZ - (Candidate.Position.Z - 1)) <=
							FMath::CeilToInt(AllowedHeightDelta))
					{
						AddResolvedWrite(
							Context,
							FIntVector(TargetXY.X, TargetXY.Y, SurfaceZ + 1),
							AmountScale);
					}
				});
		}
	}

	// 메시 풋프린트의 X/Y를 합친 뒤 각 열의 최상단 유효 표면을 해석합니다.
	bool ResolveStaticMeshFootprints(
		UWorld* World,
		AActor* TraceOwner,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelDepositBuildContext& Context)
	{
		// 겹친 풋프린트의 X/Y 트레이스는 한 번으로 합칩니다.
		TMap<FIntPoint, FDRFootprintTraceTarget> UniqueTargets;
		const float MaximumSlopeTangent = FMath::Tan(FMath::DegreesToRadians(
			FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 89.f)));
		for (const FDRSurfaceCandidate& Candidate : Context.SelectedCandidates)
		{
			if (Candidate.Source != EDRDepositSurfaceSource::StaticMesh)
			{
				continue;
			}

			ForEachFootprintCell(
				Context,
				Candidate,
				MaximumSlopeTangent,
				[&](const FIntPoint& TargetXY, float AmountScale, float AllowedHeightDelta)
				{
					const float CenterSurfaceZ = static_cast<float>(Candidate.Position.Z - 1);
					if (FDRFootprintTraceTarget* Existing = UniqueTargets.Find(TargetXY))
					{
						Existing->AmountScale = FMath::Max(Existing->AmountScale, AmountScale);
						Existing->MinLocalSurfaceZ = FMath::Min(
							Existing->MinLocalSurfaceZ,
							CenterSurfaceZ - AllowedHeightDelta);
						Existing->MaxLocalSurfaceZ = FMath::Max(
							Existing->MaxLocalSurfaceZ,
							CenterSurfaceZ + AllowedHeightDelta);
					}
					else
					{
						FDRFootprintTraceTarget Target;
						Target.VoxelXY = TargetXY;
						Target.AmountScale = AmountScale;
						Target.MinLocalSurfaceZ = CenterSurfaceZ - AllowedHeightDelta;
						Target.MaxLocalSurfaceZ = CenterSurfaceZ + AllowedHeightDelta;
						UniqueTargets.Add(TargetXY, Target);
					}
				});
		}

		if (UniqueTargets.IsEmpty())
		{
			return true;
		}
		if (!IsValid(World) || !Command.bDepositOnStaticMeshes)
		{
			return false;
		}

		// 좌표순으로 정렬해 모든 인스턴스의 트레이스 순서를 고정합니다.
		TArray<FDRFootprintTraceTarget> Targets;
		UniqueTargets.GenerateValueArray(Targets);
		Targets.Sort([](const FDRFootprintTraceTarget& A, const FDRFootprintTraceTarget& B)
		{
			return A.VoxelXY.X != B.VoxelXY.X
				? A.VoxelXY.X < B.VoxelXY.X
				: A.VoxelXY.Y < B.VoxelXY.Y;
		});

		FCollisionObjectQueryParams ObjectParams;
		FCollisionQueryParams QueryParams;
		MakeStaticTraceParameters(
			TraceOwner,
			Command.bTraceComplexStaticMeshSurfaces,
			ObjectParams,
			QueryParams);
		const float MinimumNormalZ = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 90.f)));
		const float TraceTopZ = Command.AreaCenter.Z + FMath::Abs(Command.AreaExtent.Z);
		const float TraceBottomZ = Command.AreaCenter.Z - FMath::Abs(Command.AreaExtent.Z);
		for (const FDRFootprintTraceTarget& Target : Targets)
		{
			const FVector TargetWorld = Context.VoxelWorld->LocalToGlobalFloatBP(FVector(
				static_cast<float>(Target.VoxelXY.X),
				static_cast<float>(Target.VoxelXY.Y),
				0.f));
			TArray<FHitResult> Hits;
			World->LineTraceMultiByObjectType(
				Hits,
				FVector(TargetWorld.X, TargetWorld.Y, TraceTopZ),
				FVector(TargetWorld.X, TargetWorld.Y, TraceBottomZ),
				ObjectParams,
				QueryParams);
			const FHitResult* TopHit = FindTopWorldStaticHit(Hits);
			if (TopHit == nullptr || !IsEligibleStaticMeshDepositHit(
				*TopHit,
				MinimumNormalZ,
				Command.RequiredStaticMeshSurfaceTag))
			{
				continue;
			}

			const FVoxelVector LocalSurface =
				Context.VoxelWorld->GlobalToLocalFloat(TopHit->ImpactPoint);
			const float LocalSurfaceZ = static_cast<float>(LocalSurface.Z);
			if (!FMath::IsFinite(LocalSurfaceZ) ||
				LocalSurfaceZ < Target.MinLocalSurfaceZ ||
				LocalSurfaceZ > Target.MaxLocalSurfaceZ)
			{
				continue;
			}

			// 경계 오차를 줄이도록 접촉 기준을 메시 표면보다 조금 낮춥니다.
			const float ContactSurfaceZ = LocalSurfaceZ - DRStaticMeshContactInsetVoxels;
			const int64 CandidateZ64 = FMath::FloorToInt64(ContactSurfaceZ) + 1;
			if (CandidateZ64 <= Context.WriteVoxelMin.Z ||
				CandidateZ64 > Context.WriteVoxelMax.Z)
			{
				continue;
			}

			AddResolvedWrite(
				Context,
				FIntVector(
					Target.VoxelXY.X,
					Target.VoxelXY.Y,
					static_cast<int32>(CandidateZ64)),
				Target.AmountScale,
				&ContactSurfaceZ);
		}
		return true;
	}

	// 좌표순 정렬 후 섞어 결정적인 쓰기 배열을 만듭니다.
	void FinalizePlan(
		FDRVoxelDepositBuildContext& Context,
		FDRVoxelDepositPlan& OutPlan)
	{
		OutPlan.Settings = Context.Settings;
		OutPlan.WriteVoxelMin = Context.WriteVoxelMin;
		OutPlan.WriteVoxelMax = Context.WriteVoxelMax;
		Context.WritesByPosition.GenerateValueArray(OutPlan.Writes);
		// TMap 순회 순서를 제거해 적용 순서를 결정적으로 만듭니다.
		OutPlan.Writes.Sort([](const FDRVoxelDepositWrite& A, const FDRVoxelDepositWrite& B)
		{
			if (A.Position.X != B.Position.X)
			{
				return A.Position.X < B.Position.X;
			}
			if (A.Position.Y != B.Position.Y)
			{
				return A.Position.Y < B.Position.Y;
			}
			return A.Position.Z < B.Position.Z;
		});
		DRVoxelDeposit::ShuffleArray(OutPlan.Writes, Context.RandomStream);
	}

	// 값과 머터리얼을 기록하고 실제 변경 범위를 누적합니다.
	void RecordDepositVoxel(
		FVoxelData& Data,
		const FVoxelMaterial& Material,
		const FIntVector& Position,
		float NewValue,
		TSet<FIntVector>& WrittenPositions,
		FVoxelIntBoxWithValidity& ModifiedBounds)
	{
		WrittenPositions.Add(Position);
		Data.SetValue(Position, FVoxelValue(NewValue));
		Data.SetMaterial(Position, Material);
		ModifiedBounds += Position;
	}

	// 준비 후 데이터가 바뀔 수 있으므로 적용 직전에 조건을 다시 확인합니다.
	void TryApplyWrite(
		const FDRVoxelDepositPlan& Plan,
		const FDRVoxelDepositWrite& Write,
		FVoxelData& Data,
		const FVoxelMaterial& Material,
		TSet<FIntVector>& WrittenPositions,
		FVoxelIntBoxWithValidity& ModifiedBounds)
	{
		const float DepositAmount =
			Plan.Settings.DepositAmountPerPass * FMath::Max(0.f, Write.AmountScale);
		if (DepositAmount <= SMALL_NUMBER ||
			!IsInsideBounds(Write.Position, Plan.WriteVoxelMin, Plan.WriteVoxelMax) ||
			Write.Position.Z <= Plan.WriteVoxelMin.Z ||
			WrittenPositions.Contains(Write.Position))
		{
			return;
		}

		// 복셀 퇴적은 아래 고체가 필요하며 메시 지지 쓰기는 예외입니다.
		const FIntVector BelowPosition(
			Write.Position.X,
			Write.Position.Y,
			Write.Position.Z - 1);
		const float BelowValue = Data.GetValue(BelowPosition, 0).ToFloat();
		if (BelowValue > 0.f && !Write.bHasStaticMeshSupport)
		{
			return;
		}

		// 이미 고체인 목표 복셀은 건너뜁니다.
		const float CurrentValue = Data.GetValue(Write.Position, 0).ToFloat();
		if (CurrentValue <= 0.f)
		{
			return;
		}

		float DepositStartValue = CurrentValue;
		if (Write.bHasStaticMeshSupport)
		{
			// 메시 높이에 맞춰 시작 밀도를 제한합니다.
			DepositStartValue = FMath::Min(
				DepositStartValue,
				FMath::Clamp(
					static_cast<float>(Write.Position.Z) - Write.StaticMeshSurfaceZ,
					0.f,
					1.f));

			// 메시 아래가 비어 있으면 얇은 접촉 지지층을 만듭니다.
			if (BelowValue > 0.f &&
				IsInsideBounds(BelowPosition, Plan.WriteVoxelMin, Plan.WriteVoxelMax) &&
				!WrittenPositions.Contains(BelowPosition))
			{
				const float BaseSupportValue = FMath::Clamp(
					static_cast<float>(BelowPosition.Z) - Write.StaticMeshSurfaceZ,
					-1.f,
					0.f);
				RecordDepositVoxel(
					Data,
					Material,
					BelowPosition,
					FMath::Clamp(BaseSupportValue - DepositAmount, -1.f, 1.f),
					WrittenPositions,
					ModifiedBounds);
			}
		}

		const float NewValue = FMath::Clamp(
			DepositStartValue - DepositAmount,
			-1.f,
			1.f);
		if (!FMath::IsNearlyEqual(CurrentValue, NewValue))
		{
			RecordDepositVoxel(
				Data,
				Material,
				Write.Position,
				NewValue,
				WrittenPositions,
				ModifiedBounds);
		}
	}
}

bool FDRVoxelDepositOperations::PrepareDepositCommand(
	UWorld* World,
	AVoxelWorld* VoxelWorld,
	AActor* TraceOwner,
	const FDRVoxelDepositCommand& Command,
	FDRVoxelDepositPlan& OutPlan)
{
	// 실패 시 이전 결과가 남지 않도록 출력 계획을 먼저 비웁니다.
	OutPlan.Reset();
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	// 1. 명령을 검증하고 표본 및 메시 후보를 수집합니다.
	FDRVoxelDepositBuildContext Context;
	if (!InitializeBuildContext(VoxelWorld, Command, Context) ||
		!CollectStaticMeshCandidates(World, TraceOwner, Command, Context))
	{
		return false;
	}

	// 2. 복셀 후보를 합치고 낮은 최상단 후보부터 선택합니다.
	CollectVoxelCandidates(Context);
	SelectCandidates(Context);
	// 표면이 없는 빈 계획도 정상 결과입니다.
	if (Context.SelectedCandidates.IsEmpty())
	{
		return true;
	}

	// 3. 선택한 후보를 표면 종류에 맞는 풋프린트로 확장합니다.
	ResolveVoxelFootprints(Context);
	if (!ResolveStaticMeshFootprints(World, TraceOwner, Command, Context))
	{
		return false;
	}

	// 4. 중복을 제거한 쓰기를 다음 프레임용 계획으로 옮깁니다.
	FinalizePlan(Context, OutPlan);
	return true;
}

bool FDRVoxelDepositOperations::ApplyDepositPlan(
	AVoxelWorld* VoxelWorld,
	FDRVoxelDepositPlan& Plan)
{
	// 빈 계획은 정상적으로 완료합니다.
	if (Plan.IsEmpty())
	{
		Plan.Reset();
		return true;
	}
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		Plan.Reset();
		return false;
	}

	if (!IsSafeInclusiveVoxelBounds(
		Plan.WriteVoxelMin,
		Plan.WriteVoxelMax))
	{
		Plan.Reset();
		return false;
	}

	// 실제 쓰기 좌표와 바로 아래 지지 셀만 잠급니다.
	FVoxelIntBoxWithValidity LockBounds;
	for (const FDRVoxelDepositWrite& Write : Plan.Writes)
	{
		LockBounds += Write.Position;
		LockBounds += FIntVector(
			Write.Position.X,
			Write.Position.Y,
			Write.Position.Z - 1);
	}

	// 모든 쓰기에 같은 머터리얼을 사용합니다.
	FVoxelMaterial Material;
	Material.SetSingleIndex(Plan.Settings.DepositMaterialIndex);
	FVoxelIntBoxWithValidity ModifiedBounds;
	TSet<FIntVector> WrittenPositions;
	WrittenPositions.Reserve(Plan.Writes.Num() * 2);
	FVoxelData& Data = VoxelWorld->GetData();
	{
		// 하나의 쓰기 잠금 안에서 계획 전체를 적용합니다.
		FVoxelWriteScopeLock Lock(Data, LockBounds.GetBox(), FUNCTION_FNAME);
		for (const FDRVoxelDepositWrite& Write : Plan.Writes)
		{
			TryApplyWrite(
				Plan,
				Write,
				Data,
				Material,
				WrittenPositions,
				ModifiedBounds);
		}
	}

	// 실제 변경 영역과 인접 한 칸만 갱신합니다.
	if (ModifiedBounds.IsValid())
	{
		UVoxelBlueprintLibrary::UpdateBounds(
			VoxelWorld,
			ModifiedBounds.GetBox().Extend(1));
	}

	// 재적용을 막기 위해 완료된 계획을 비웁니다.
	Plan.Reset();
	return true;
}
