#include "DRVoxelTerrainOperationLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "VoxelWorld.h"

namespace
{
	constexpr float DRStaticMeshSampleJitterRatio = 0.4f;
	constexpr float DRStaticMeshFootprintEdgeStrength = 0.55f;
	// 실제 메시 표면과 생성되는 복셀 등가면 사이의 작은 틈을 막는다.
	constexpr float DRStaticMeshContactInsetVoxels = 0.1f;

	struct FDRFootprintTraceTarget
	{
		FIntPoint VoxelXY = FIntPoint::ZeroValue;
		float AmountScale = 1.f;
		float MinLocalSurfaceZ = 0.f;
		float MaxLocalSurfaceZ = 0.f;
	};

	bool IsEligibleStaticMeshDepositHit(
		const FHitResult& Hit,
		float MinimumSurfaceNormalZ,
		FName RequiredSurfaceTag)
	{
		// WorldStatic에는 다른 컴포넌트도 포함될 수 있으므로 StaticMesh, Mobility, 경사, 태그를 모두 확인한다.
		const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
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

	bool CollectStaticMeshSurfaceCandidates(
		UWorld* World,
		AVoxelWorld* VoxelWorld,
		AActor* TraceOwner,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelDepositInBoxRequest& Request)
	{
		if (!Command.bDepositOnStaticMeshes)
		{
			return true;
		}

		if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
			!FMath::IsFinite(Command.Settings.SurfaceSampleSpacing) ||
			Command.Settings.SurfaceSampleSpacing <= 0.f ||
			Command.ScanCenter.ContainsNaN() || Command.ScanExtent.ContainsNaN())
		{
			return false;
		}

		const FVector ScanExtent(
			FMath::Abs(Command.ScanExtent.X),
			FMath::Abs(Command.ScanExtent.Y),
			FMath::Abs(Command.ScanExtent.Z));
		const double SampleSpacing = static_cast<double>(Command.Settings.SurfaceSampleSpacing);
		const int64 ColumnCountX = FMath::FloorToInt64(ScanExtent.X * 2.0 / SampleSpacing) + 1;
		const int64 ColumnCountY = FMath::FloorToInt64(ScanExtent.Y * 2.0 / SampleSpacing) + 1;
		if (ScanExtent.Z <= KINDA_SMALL_NUMBER ||
			ColumnCountX <= 0 || ColumnCountY <= 0 ||
			ColumnCountX > MAX_int32 || ColumnCountY > MAX_int32 ||
			ColumnCountX > MAX_int32 / ColumnCountY)
		{
			return false;
		}

		FRandomStream RandomStream(Command.Settings.RandomSeed ^ 0x5A17C9E3);
		TArray<int32> ColumnOrder;
		DRVoxelTerrain::BuildRandomUniqueIndices(
			static_cast<int32>(ColumnCountX * ColumnCountY),
			Command.Settings.RandomSurfaceSampleCount,
			RandomStream,
			ColumnOrder);

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(DRStaticMeshDepositSurface),
			Command.bTraceComplexStaticMeshSurfaces);
		if (TraceOwner != nullptr)
		{
			QueryParams.AddIgnoredActor(TraceOwner);
		}

		const float MinimumSurfaceNormalZ = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 90.f)));
		const FVector BoxMin = Command.ScanCenter - ScanExtent;
		const FVector BoxMax = Command.ScanCenter + ScanExtent;
		const float JitterRadius = Command.Settings.SurfaceSampleSpacing * DRStaticMeshSampleJitterRatio;
		TArray<FVector> SurfaceHitPositions;

		// 선택된 열을 위에서 아래로 검사한다. 첫 유효 히트만 채택하므로 천장 아래 바닥에는 쌓이지 않는다.
		for (const int32 LinearIndex : ColumnOrder)
		{
			const int32 ColumnX = LinearIndex / static_cast<int32>(ColumnCountY);
			const int32 ColumnY = LinearIndex % static_cast<int32>(ColumnCountY);
			const float BaseX = BoxMin.X + ColumnX * Command.Settings.SurfaceSampleSpacing;
			const float BaseY = BoxMin.Y + ColumnY * Command.Settings.SurfaceSampleSpacing;
			const float SampleX = BaseX + RandomStream.FRandRange(
				FMath::Max(-JitterRadius, BoxMin.X - BaseX),
				FMath::Min(JitterRadius, BoxMax.X - BaseX));
			const float SampleY = BaseY + RandomStream.FRandRange(
				FMath::Max(-JitterRadius, BoxMin.Y - BaseY),
				FMath::Min(JitterRadius, BoxMax.Y - BaseY));

			TArray<FHitResult> Hits;
			World->LineTraceMultiByObjectType(
				Hits,
				FVector(SampleX, SampleY, BoxMax.Z),
				FVector(SampleX, SampleY, BoxMin.Z),
				ObjectQueryParams,
				QueryParams);
			for (const FHitResult& Hit : Hits)
			{
				if (IsEligibleStaticMeshDepositHit(
					Hit,
					MinimumSurfaceNormalZ,
					Command.RequiredStaticMeshSurfaceTag))
				{
					SurfaceHitPositions.Add(Hit.ImpactPoint);
					break;
				}
			}
		}

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
			Request,
			SurfaceHitPositions,
			AddedCandidateCount);
		return true;
	}

	bool ResolveStaticMeshFootprints(
		UWorld* World,
		AVoxelWorld* VoxelWorld,
		AActor* TraceOwner,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelDepositInBoxRequest& Request)
	{
		if (Request.ExternalCandidatePositions.Num() == 0)
		{
			Request.bExternalFootprintsResolved = true;
			return true;
		}

		if (!Command.bDepositOnStaticMeshes || !IsValid(World) ||
			!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
		{
			Request.ExternalCandidatePositions.Reset();
			Request.ExternalSupportSurfaceZByVoxel.Reset();
			Request.ExternalResolvedAmountScaleByVoxel.Reset();
			Request.bExternalFootprintsResolved = true;
			return false;
		}

		const int32 Radius = Request.DepositFootprintRadius;
		const float MaximumSlopeTangent = FMath::Tan(FMath::DegreesToRadians(
			FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 89.f)));
		TMap<FIntPoint, FDRFootprintTraceTarget> UniqueTargets;

		// 겹치는 원형 풋프린트 셀은 하나로 합쳐 물리 검사를 중복하지 않는다.
		for (const FIntVector& Center : Request.ExternalCandidatePositions)
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
					if (TargetX64 < Request.WriteVoxelMin.X || TargetX64 > Request.WriteVoxelMax.X ||
						TargetY64 < Request.WriteVoxelMin.Y || TargetY64 > Request.WriteVoxelMax.Y)
					{
						continue;
					}

					const FIntPoint TargetXY(
						static_cast<int32>(TargetX64),
						static_cast<int32>(TargetY64));
					const float CenterSurfaceZ = static_cast<float>(Center.Z - 1);
					if (FDRFootprintTraceTarget* ExistingTarget = UniqueTargets.Find(TargetXY))
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
						FDRFootprintTraceTarget Target;
						Target.VoxelXY = TargetXY;
						Target.AmountScale = AmountScale;
						Target.MinLocalSurfaceZ = CenterSurfaceZ - AllowedHeightDelta;
						Target.MaxLocalSurfaceZ = CenterSurfaceZ + AllowedHeightDelta;
						UniqueTargets.Add(TargetXY, Target);
					}
				}
			}
		}

		TArray<FDRFootprintTraceTarget> Targets;
		UniqueTargets.GenerateValueArray(Targets);
		Targets.Sort([](const FDRFootprintTraceTarget& A, const FDRFootprintTraceTarget& B)
		{
			return A.VoxelXY.X != B.VoxelXY.X
				? A.VoxelXY.X < B.VoxelXY.X
				: A.VoxelXY.Y < B.VoxelXY.Y;
		});

		Request.ExternalSupportSurfaceZByVoxel.Reset();
		Request.ExternalResolvedAmountScaleByVoxel.Reset();
		Request.bExternalFootprintsResolved = false;

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(DRStaticMeshDepositFootprint),
			Command.bTraceComplexStaticMeshSurfaces);
		if (TraceOwner != nullptr)
		{
			QueryParams.AddIgnoredActor(TraceOwner);
		}
		const float MinimumSurfaceNormalZ = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 90.f)));
		const FVector AreaExtent(
			FMath::Abs(Command.AreaExtent.X),
			FMath::Abs(Command.AreaExtent.Y),
			FMath::Abs(Command.AreaExtent.Z));
		const float TraceTopZ = Command.AreaCenter.Z + AreaExtent.Z;
		const float TraceBottomZ = Command.AreaCenter.Z - AreaExtent.Z;

		for (const FDRFootprintTraceTarget& Target : Targets)
		{
			const FVector TargetWorldPosition = VoxelWorld->LocalToGlobalFloatBP(FVector(
				static_cast<float>(Target.VoxelXY.X),
				static_cast<float>(Target.VoxelXY.Y),
				0.f));
			TArray<FHitResult> Hits;
			World->LineTraceMultiByObjectType(
				Hits,
				FVector(TargetWorldPosition.X, TargetWorldPosition.Y, TraceTopZ),
				FVector(TargetWorldPosition.X, TargetWorldPosition.Y, TraceBottomZ),
				ObjectQueryParams,
				QueryParams);

			for (const FHitResult& Hit : Hits)
			{
				if (!IsEligibleStaticMeshDepositHit(
					Hit,
					MinimumSurfaceNormalZ,
					Command.RequiredStaticMeshSurfaceTag))
				{
					continue;
				}

				const FVoxelVector LocalSurfacePosition = VoxelWorld->GlobalToLocalFloat(Hit.ImpactPoint);
				const float LocalSurfaceZ = static_cast<float>(LocalSurfacePosition.Z);
				if (!FMath::IsFinite(LocalSurfaceZ) ||
					LocalSurfaceZ < Target.MinLocalSurfaceZ ||
					LocalSurfaceZ > Target.MaxLocalSurfaceZ)
				{
					continue;
				}

				const float ContactSurfaceZ = LocalSurfaceZ - DRStaticMeshContactInsetVoxels;
				const int64 CandidateZ64 = FMath::FloorToInt64(ContactSurfaceZ) + 1;
				if (CandidateZ64 <= Request.WriteVoxelMin.Z || CandidateZ64 > Request.WriteVoxelMax.Z)
				{
					continue;
				}

				const FIntVector DepositPosition(
					Target.VoxelXY.X,
					Target.VoxelXY.Y,
					static_cast<int32>(CandidateZ64));
				float& StoredSurfaceZ = Request.ExternalSupportSurfaceZByVoxel.FindOrAdd(
					DepositPosition,
					ContactSurfaceZ);
				StoredSurfaceZ = FMath::Max(StoredSurfaceZ, ContactSurfaceZ);
				float& StoredAmountScale = Request.ExternalResolvedAmountScaleByVoxel.FindOrAdd(
					DepositPosition,
					Target.AmountScale);
				StoredAmountScale = FMath::Max(StoredAmountScale, Target.AmountScale);
				break;
			}
		}

		Request.bExternalFootprintsResolved = true;
		return true;
	}
}

bool UDRVoxelTerrainOperationLibrary::MakeDepositCommand(
	const FDRVoxelTerrainOperationContext& Context,
	FDRVoxelDepositCommand& OutCommand)
{
	OutCommand = FDRVoxelDepositCommand();
	if (!IsValid(Context.World) || !IsValid(Context.VoxelWorld) ||
		!Context.VoxelWorld->IsCreated() || Context.DepositSettings == nullptr ||
		Context.AreaCenter.ContainsNaN() || Context.AreaExtent.ContainsNaN() ||
		!FMath::IsFinite(Context.RandomScanWorldSize) || Context.RandomScanWorldSize <= 0.f)
	{
		return false;
	}

	const FVector AreaExtent(
		FMath::Abs(Context.AreaExtent.X),
		FMath::Abs(Context.AreaExtent.Y),
		FMath::Abs(Context.AreaExtent.Z));
	if (AreaExtent.X <= KINDA_SMALL_NUMBER ||
		AreaExtent.Y <= KINDA_SMALL_NUMBER ||
		AreaExtent.Z <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCommand.Settings = *Context.DepositSettings;
	OutCommand.Settings.RandomSeed = FMath::Rand();
	const float RequestedHalfSize = Context.RandomScanWorldSize * 0.5f;
	OutCommand.ScanExtent = FVector(
		FMath::Min(AreaExtent.X, RequestedHalfSize),
		FMath::Min(AreaExtent.Y, RequestedHalfSize),
		AreaExtent.Z);
	FRandomStream ScanAreaRandomStream(OutCommand.Settings.RandomSeed ^ 0x27D4EB2D);
	OutCommand.ScanCenter = Context.AreaCenter + FVector(
		ScanAreaRandomStream.FRandRange(
			-(AreaExtent.X - OutCommand.ScanExtent.X),
			AreaExtent.X - OutCommand.ScanExtent.X),
		ScanAreaRandomStream.FRandRange(
			-(AreaExtent.Y - OutCommand.ScanExtent.Y),
			AreaExtent.Y - OutCommand.ScanExtent.Y),
		0.f);
	OutCommand.AreaCenter = Context.AreaCenter;
	OutCommand.AreaExtent = AreaExtent;
	OutCommand.RequiredStaticMeshSurfaceTag = Context.RequiredStaticMeshSurfaceTag;
	OutCommand.MaxStaticMeshSlopeAngle = Context.MaxStaticMeshSlopeAngle;
	OutCommand.bDepositOnStaticMeshes = Context.bDepositOnStaticMeshes;
	OutCommand.bBlockDepositBelowStaticMeshes = Context.bBlockDepositBelowStaticMeshes;
	OutCommand.bTraceComplexStaticMeshSurfaces = Context.bTraceComplexStaticMeshSurfaces;
	return true;
}

bool UDRVoxelTerrainOperationLibrary::ExecuteDepositCommand(
	UWorld* World,
	AVoxelWorld* VoxelWorld,
	AActor* TraceOwner,
	const FDRVoxelDepositCommand& Command,
	FDRVoxelTerrainOperationResult& OutResult)
{
	OutResult = FDRVoxelTerrainOperationResult();
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FDRVoxelDepositInBoxRequest Request;
	if (!MakeDepositInBoxRequest(
		VoxelWorld,
		Command.ScanCenter,
		Command.ScanExtent,
		Command.Settings,
		Request) ||
		!ConfigureDepositRequestWriteBounds(
			Request,
			Command.AreaCenter,
			Command.AreaExtent))
	{
		return false;
	}

	Request.WorldBoxTopZ = Command.AreaCenter.Z + FMath::Abs(Command.AreaExtent.Z);
	Request.bBlockDepositBelowWorldStatic = Command.bBlockDepositBelowStaticMeshes;
	Request.bTraceComplexWorldStaticOcclusion = Command.bTraceComplexStaticMeshSurfaces;
	CollectStaticMeshSurfaceCandidates(World, VoxelWorld, TraceOwner, Command, Request);

	int32 ModifiedVoxelCount = 0;
	int32 ScannedColumnCount = 0;
	if (!ProcessDepositInBoxRequestPhase(Request, ModifiedVoxelCount, ScannedColumnCount))
	{
		return false;
	}
	OutResult.ScannedColumnCount += ScannedColumnCount;

	if (Request.Phase == EDRVoxelDepositRequestPhase::ResolveFootprints)
	{
		ResolveStaticMeshFootprints(World, VoxelWorld, TraceOwner, Command, Request);
		if (!ProcessDepositInBoxRequestPhase(Request, ModifiedVoxelCount, ScannedColumnCount))
		{
			return false;
		}
	}

	if (Request.Phase == EDRVoxelDepositRequestPhase::ApplyVoxels)
	{
		if (!ProcessDepositInBoxRequestPhase(Request, ModifiedVoxelCount, ScannedColumnCount))
		{
			return false;
		}
		OutResult.ModifiedVoxelCount += ModifiedVoxelCount;
	}

	return Request.Phase == EDRVoxelDepositRequestPhase::Finished;
}
