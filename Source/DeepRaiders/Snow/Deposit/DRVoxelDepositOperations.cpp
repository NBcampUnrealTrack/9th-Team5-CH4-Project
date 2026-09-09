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
	constexpr float DRDepositFootprintEdgeStrength = 0.55f;
	// 메시 경계 오차를 줄이기 위한 접촉면 보정값입니다.
	constexpr float DRStaticMeshContactInsetVoxels = 0.1f;
	constexpr float DRDepositMaximumFootprintSlopeDegrees = 55.f;
	constexpr int32 DRMaximumRandomSurfaceSamples = 4096;
	constexpr int64 DRMaximumDepositFootprintWorkItems = 4096;

	// 표면 후보를 찾은 방식을 구분합니다.
	enum class EDRDepositSurfaceSource : uint8
	{
		Voxel,
		StaticMesh
	};

	struct FDRSurfaceCandidate
	{
		FIntVector Position = FIntVector::ZeroValue;
		EDRDepositSurfaceSource Source = EDRDepositSurfaceSource::Voxel;
	};

	struct FDRFootprintTraceTarget
	{
		FIntPoint VoxelXY = FIntPoint::ZeroValue;
		float AmountScale = 1.f;
		float MinLocalSurfaceZ = 0.f;
		float MaxLocalSurfaceZ = 0.f;
	};

	struct FDRVoxelDepositBuildContext
	{
		AVoxelWorld* VoxelWorld = nullptr;
		const FDRVoxelDepositCommand* Command = nullptr;
		FIntVector CoreVoxelMin = FIntVector::ZeroValue;
		FIntVector CoreVoxelMax = FIntVector::ZeroValue;
		FIntVector WriteVoxelMin = FIntVector::ZeroValue;
		FIntVector WriteVoxelMax = FIntVector::ZeroValue;
		FDRVoxelDepositInBoxSettings Settings;
		int32 DepositFootprintRadius = 0;
		FRandomStream RandomStream;
		TArray<FIntPoint> ScanSamplePositions;
		TMap<FIntPoint, FDRSurfaceCandidate> TopCandidateByColumn;
		TArray<FDRSurfaceCandidate> SelectedCandidates;
		TMap<FIntPoint, int32> TopSurfaceZByColumn;
		TMap<FIntVector, FDRVoxelDepositWrite> WritesByPosition;
		float WorldBoxTopZ = 0.f;
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

	bool IsInsideBounds(
		const FIntVector& Position,
		const FIntVector& BoundsMin,
		const FIntVector& BoundsMax)
	{
		return Position.X >= BoundsMin.X && Position.X <= BoundsMax.X &&
			Position.Y >= BoundsMin.Y && Position.Y <= BoundsMax.Y &&
			Position.Z >= BoundsMin.Z && Position.Z <= BoundsMax.Z;
	}

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

		//TODO! 격자를 계속 사용할지 확인 필요

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
		OutContext.Command = &Command;
		OutContext.Settings = SanitizeDepositSettings(Command.Settings);
		OutContext.RandomStream.Initialize(OutContext.Settings.RandomSeed);
		OutContext.WorldBoxTopZ = Command.AreaCenter.Z + AreaExtent.Z;
		OutContext.bTraceComplexWorldStatic = Command.bTraceComplexStaticMeshSurfaces;

		GetLocalVoxelBoundsForWorldBox(
			VoxelWorld,
			Command.ScanCenter,
			ScanExtent,
			OutContext.CoreVoxelMin,
			OutContext.CoreVoxelMax);

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

	// Z축 위에서 아래로 첫 빈 공간-고체 경계를 찾는 부분
	int32 FindTopSurfaceZ(
		FVoxelData& Data,
		int32 X,
		int32 Y,
		int32 MinZ,
		int32 MaxZ)
	{
		float AboveValue = Data.GetValue(FIntVector(X, Y, MaxZ), 0).ToFloat();
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

	// 후보와 퍼진 퇴적 위치 모두 동일한 모양 경계로 제한합니다.
	bool IsInsideDepositArea(const FDRVoxelDepositBuildContext& Context, const FIntVector& Position)
	{
		const FVector WorldPosition = Context.VoxelWorld->LocalToGlobalFloatBP(FVector(Position));
		return Context.Command->ContainsWorldPosition(WorldPosition);
	}

	void OfferSurfaceCandidate(
		FDRVoxelDepositBuildContext& Context,
		const FDRSurfaceCandidate& Candidate)
	{
		if (!IsInsideBounds(
			Candidate.Position,
			Context.CoreVoxelMin,
			Context.CoreVoxelMax) || !IsInsideDepositArea(Context, Candidate.Position))
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

	void SelectCandidates(FDRVoxelDepositBuildContext& Context)
	{
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

	}

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

	void AddResolvedWrite(
		FDRVoxelDepositBuildContext& Context,
		const FIntVector& Position,
		float AmountScale,
		const float* StaticMeshSurfaceZ = nullptr,
		UPrimitiveComponent* SupportComponent = nullptr)
	{
		if (!IsInsideBounds(Position, Context.WriteVoxelMin, Context.WriteVoxelMax) ||
			Position.Z <= Context.WriteVoxelMin.Z || !IsInsideDepositArea(Context, Position))
		{
			return;
		}

		FDRVoxelDepositWrite* Existing = Context.WritesByPosition.Find(Position);
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
			NewWrite.bAllowBelowSupport = IsInsideDepositArea(
				Context, Position - FIntVector(0, 0, 1));
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
			Write->SupportComponent = SupportComponent;
		}
	}

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
				&ContactSurfaceZ,
				TopHit->GetComponent());
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
		FVoxelIntBoxWithValidity& ModifiedBounds,
		TArray<FDRVoxelDepositCell>& ChangedCells,
		bool bPaintMaterial = true)
	{
		const FVoxelValue Value(NewValue);
		if (Data.GetValue(Position, 0) == Value)
		{
			return;
		}
		WrittenPositions.Add(Position);
		Data.SetValue(Position, Value);
		if (bPaintMaterial)
		{
			Data.SetMaterial(Position, Material);
		}
		FDRVoxelDepositCell& Cell = ChangedCells.AddDefaulted_GetRef();
		Cell.Position = Position;
		Cell.Value = Value.GetStorage();
		Cell.bPaintMaterial = bPaintMaterial;
		ModifiedBounds += Position;
	}

	// 준비 후 데이터가 바뀔 수 있으므로 적용 직전에 조건을 다시 확인합니다.
	void TryApplyWrite(
		const FDRVoxelDepositPlan& Plan,
		const FDRVoxelDepositWrite& Write,
		FVoxelData& Data,
		const FVoxelMaterial& Material,
		TSet<FIntVector>& WrittenPositions,
		FVoxelIntBoxWithValidity& ModifiedBounds,
		TArray<FDRVoxelDepositCell>& ChangedCells)
	{
		const float DepositAmount = FMath::Min(0.25f,
			Plan.Settings.DepositAmountPerPass * FMath::Max(0.f, Write.AmountScale));
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

		const float SurfaceHeight = BelowValue <= 0.f
			? BelowPosition.Z + (-BelowValue) / (CurrentValue - BelowValue)
			: Write.StaticMeshSurfaceZ;
		const float NewHeight = SurfaceHeight + DepositAmount;
		// 두 셀의 연속적인 표면 값을 함께 낮춥니다. 기존 고체는 재질을 바꾸지 않습니다.
		if (Write.bAllowBelowSupport && !WrittenPositions.Contains(BelowPosition))
		{
			RecordDepositVoxel(Data, Material, BelowPosition,
				FMath::Min(BelowValue, FMath::Clamp(BelowPosition.Z - NewHeight, -1.f, 1.f)),
				WrittenPositions, ModifiedBounds, ChangedCells, BelowValue > 0.f);
		}
		const float NewValue = FMath::Min(CurrentValue,
			FMath::Clamp(Write.Position.Z - NewHeight, -1.f, 1.f));
		if (!FMath::IsNearlyEqual(CurrentValue, NewValue))
		{
			RecordDepositVoxel(
				Data,
				Material,
				Write.Position,
				NewValue,
				WrittenPositions,
				ModifiedBounds,
				ChangedCells);
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

	// 4. 중복 제거 후 양옆 높이를 비교해 퇴적량을 조절합니다.
	FinalizePlan(Context, OutPlan);
	LevelDepositPlan(VoxelWorld, OutPlan);
	return true;
}

bool FDRVoxelDepositOperations::ApplyDepositPlan(
	AVoxelWorld* VoxelWorld,
	FDRVoxelDepositPlan& Plan,
	FDRVoxelDepositResult& OutResult)
{
	OutResult = FDRVoxelDepositResult();
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
	OutResult.VoxelWorldName = VoxelWorld->GetFName();
	OutResult.MaterialIndex = Plan.Settings.DepositMaterialIndex;
	FVoxelIntBoxWithValidity ModifiedBounds;
	TSet<FIntVector> WrittenPositions;
	WrittenPositions.Reserve(Plan.Writes.Num() * 2);
	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelWriteScopeLock Lock(Data, LockBounds.GetBox(), FUNCTION_FNAME);
		for (const FDRVoxelDepositWrite& Write : Plan.Writes)
		{
			TryApplyWrite(
				Plan,
				Write,
				Data,
				Material,
				WrittenPositions,
				ModifiedBounds,
				OutResult.Cells);
		}
	}

	// 실제 변경 영역과 인접 한 칸만 갱신합니다.
	if (ModifiedBounds.IsValid())
	{
		UVoxelBlueprintLibrary::UpdateBounds(
			VoxelWorld,
			ModifiedBounds.GetBox().Extend(1));
	}

	Plan.Reset();
	return true;
}

void FDRVoxelDepositOperations::LevelDepositPlan(AVoxelWorld* VoxelWorld, FDRVoxelDepositPlan& Plan)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Plan.IsEmpty() ||
		!FMath::IsFinite(Plan.Settings.LevelingStrength) || Plan.Settings.LevelingStrength <= 0.f)
	{
		return;
	}
	struct FSurface
	{
		float Height;
		const FDRVoxelDepositWrite* Write;
	};
	TMap<FIntPoint, FSurface> Surfaces;
	FVoxelIntBoxWithValidity Bounds;
	for (const FDRVoxelDepositWrite& Write : Plan.Writes)
	{
		Bounds += Write.Position;
		Bounds += Write.Position - FIntVector(0, 0, 1);
	}
	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelReadScopeLock Lock(Data, Bounds.GetBox(), FUNCTION_FNAME);
		for (const FDRVoxelDepositWrite& Write : Plan.Writes)
		{
			const float Above = Data.GetValue(Write.Position, 0).ToFloat();
			const float Below = Data.GetValue(Write.Position - FIntVector(0, 0, 1), 0).ToFloat();
			if (Above <= 0.f || (Below > 0.f && !Write.bHasStaticMeshSupport))
			{
				continue;
			}
			const float Height = Below <= 0.f
				? Write.Position.Z - 1.f + (-Below) / (Above - Below)
				: Write.StaticMeshSurfaceZ;
			Surfaces.Add(FIntPoint(Write.Position.X, Write.Position.Y), {Height, &Write});
		}
	}

	const float MaxStep = FMath::Tan(FMath::DegreesToRadians(DRDepositMaximumFootprintSlopeDegrees));
	for (FDRVoxelDepositWrite& Write : Plan.Writes)
	{
		const FIntPoint XY(Write.Position.X, Write.Position.Y);
		const FSurface* Center = Surfaces.Find(XY);
		if (!Center)
		{
			continue;
		}
		float TargetSum = 0.f;
		int32 PairCount = 0;
		for (const FIntPoint Axis : {FIntPoint(1, 0), FIntPoint(0, 1)})
		{
			const FSurface* A = Surfaces.Find(XY - Axis);
			const FSurface* B = Surfaces.Find(XY + Axis);
			auto IsConnected = [&](const FSurface* Neighbor)
			{
				return Neighbor &&
					Neighbor->Write->bHasStaticMeshSupport == Write.bHasStaticMeshSupport &&
					Neighbor->Write->SupportComponent == Write.SupportComponent &&
					FMath::Abs(Neighbor->Height - Center->Height) <= MaxStep;
			};
			// 한쪽만 평균내면 평면 경사와 패치 경계까지 기울어집니다.
			if (IsConnected(A) && IsConnected(B))
			{
				TargetSum += (A->Height + B->Height) * 0.5f;
				++PairCount;
			}
		}
		if (PairCount > 0)
		{
			const float Difference = TargetSum / PairCount - Center->Height;
			Write.AmountScale *= FMath::Clamp(
				1.f + FMath::Clamp(Plan.Settings.LevelingStrength, 0.f, 2.f) * Difference, 0.f, 2.f);
		}
	}
}

bool FDRVoxelDepositOperations::ApplyDepositResult(AVoxelWorld* VoxelWorld, const FDRVoxelDepositResult& Result)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}
	if (Result.Cells.IsEmpty())
	{
		return true;
	}
	FVoxelIntBoxWithValidity Bounds;
	for (const FDRVoxelDepositCell& Cell : Result.Cells)
	{
		if (Cell.Value < FVoxelValue::MIN_VOXELVALUE || Cell.Value > FVoxelValue::MAX_VOXELVALUE ||
			Cell.Position.GetMin() <= MIN_int32 + 1 || Cell.Position.GetMax() >= MAX_int32 - 1)
		{
			return false;
		}
		Bounds += Cell.Position;
	}
	FVoxelMaterial Material;
	Material.SetSingleIndex(Result.MaterialIndex);
	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelWriteScopeLock Lock(Data, Bounds.GetBox(), FUNCTION_FNAME);
		for (const FDRVoxelDepositCell& Cell : Result.Cells)
		{
			Data.SetValue(Cell.Position, FVoxelValue::InternalConstructor(Cell.Value));
			if (Cell.bPaintMaterial)
			{
				Data.SetMaterial(Cell.Position, Material);
			}
		}
	}
	UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, Bounds.GetBox().Extend(1));
	return true;
}
