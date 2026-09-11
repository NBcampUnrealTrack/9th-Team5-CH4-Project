#include "DRSnowRemoveComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "DeepRaiders/Snow/DRSnowConservativeOcclusion.h"
#include "DeepRaiders/Snow/DRSnowAbsorbPlanes.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "VoxelWorld.h"

static TAutoConsoleVariable<int32> CVarSnowAbsorbStaticMeshOcclusion(
	TEXT("dr.Snow.Absorb.StaticMeshOcclusion"),
	1,
	TEXT("서버가 만든 보수적 부피 차폐를 눈 흡수에 사용한다. 0: 끔, 1: 켬"),
	ECVF_Default);

namespace
{
TArray<uint8> BuildAbsorbOcclusionDepths(
	UWorld& World, AActor& Owner, const FVector& FrustumOrigin,
	const FVector& FrustumEnd, const float InnerRadius, const float OuterRadius,
	TArray<FDRSnowAbsorbConvex>& OutVolumes)
{
	OutVolumes.Reset();
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_StaticMeshOcclusion);
	static const IConsoleVariable* LogVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("dr.Snow.Absorb.Log"));
	const int32 AbsorbOcclusionLogLevel = LogVariable ? LogVariable->GetInt() : 0;
	static uint64 NextQueryId = 0;
	const uint64 QueryId = ++NextQueryId;
	if (CVarSnowAbsorbStaticMeshOcclusion.GetValueOnGameThread() == 0)
	{
		if (AbsorbOcclusionLogLevel > 0) { UE_LOG(LogTemp, Log, TEXT("[DRSnowOcclusion] Id=%llu Disabled"), QueryId); }
		return {};
	}

	DRSnowConservativeOcclusion::FBuilder Builder;
	Builder.Origin = FrustumOrigin;
	Builder.Range = FVector::Distance(FrustumOrigin, FrustumEnd);
	Builder.Direction = (FrustumEnd - FrustumOrigin).GetSafeNormal();
	Builder.Direction.FindBestAxisVectors(Builder.AxisY, Builder.AxisZ);
	Builder.InnerRadius = InnerRadius;
	Builder.OuterRadius = OuterRadius;
	if (Builder.Range <= KINDA_SMALL_NUMBER || OuterRadius <= 0.f)
	{
		TArray<uint8> Closed;
		Closed.Init(0, DRSnowAbsorbOcclusion::SampleCount);
		return Closed;
	}
	const FBox QueryBounds = Builder.PrefixBounds(0, 0, DRSnowAbsorbOcclusion::Resolution, Builder.Range);
	struct FObstacle
	{
		UStaticMeshComponent* Mesh = nullptr;
		FBodyInstance* Body = nullptr;
		int32 Instance = INDEX_NONE;
		int32 Hits = 0;
		int32 Misses = 0;
		int32 MissingBody = 0;
	};
	TArray<FObstacle> Obstacles;
	int32 Scanned = 0, SkippedVoxel = 0, SkippedOwner = 0, SkippedNoQuery = 0, SkippedNonMesh = 0;
	int32 DetailLines = 0, DetailSuppressed = 0;
	const double GatherStarted = AbsorbOcclusionLogLevel > 0 ? FPlatformTime::Seconds() : 0.;
	const auto LogCandidate = [&](UPrimitiveComponent* Component, const TCHAR* Reason, const FBox& Bounds)
	{
		if (AbsorbOcclusionLogLevel <= 0) { return; }
		if (AbsorbOcclusionLogLevel == 1 && DetailLines >= 24) { ++DetailSuppressed; return; }
		++DetailLines;
		const AActor* Actor = Component->GetOwner();
		const USceneComponent* Parent = Component->GetAttachParent();
		const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
		UE_LOG(LogTemp, Log,
			TEXT("[DRSnowOcclusion][Candidate] Id=%llu Reason=%s Actor=%s ActorClass=%s Component=%s Class=%s Outer=%s Parent=%s Asset=%s Collision=%d Profile=%s ObjectType=%d Visible=%d Hidden=%d OriginInBounds=%d Min=%s Max=%s"),
			QueryId, Reason, *GetPathNameSafe(Actor), *GetNameSafe(Actor ? Actor->GetClass() : nullptr),
			*Component->GetPathName(), *Component->GetClass()->GetName(), *GetPathNameSafe(Component->GetOuter()),
			*GetPathNameSafe(Parent), *GetPathNameSafe(Mesh ? Mesh->GetStaticMesh() : nullptr),
			static_cast<int32>(Component->GetCollisionEnabled()), *Component->GetCollisionProfileName().ToString(),
			static_cast<int32>(Component->GetCollisionObjectType()), Component->IsVisible(), Component->bHiddenInGame != 0,
			Bounds.IsInsideOrOn(FrustumOrigin), *Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString());
	};
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components;
		It->GetComponents(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (!IsValid(Component) || !Component->IsRegistered()) { continue; }
			++Scanned;
			const FBox Bounds = Component->Bounds.GetBox();
			const bool bNearby = Bounds.IsValid && QueryBounds.Intersect(Bounds);
			const TCHAR* SkipReason = nullptr;
			bool bOwnedByAbsorber = *It == &Owner;
			for (const AActor* Actor = *It; IsValid(Actor) && !bOwnedByAbsorber; Actor = Actor->GetAttachParentActor())
			{
				bOwnedByAbsorber = Actor->GetOwner() == &Owner || Actor->GetAttachParentActor() == &Owner;
			}
			if (bOwnedByAbsorber) { ++SkippedOwner; SkipReason = TEXT("SkipAbsorberOrEquipment"); }
			else if (Component->IsA<UVoxelProceduralMeshComponent>() || It->IsA<AVoxelWorld>() ||
				Component->GetTypedOuter<AVoxelWorld>() != nullptr)
			{
				++SkippedVoxel; SkipReason = TEXT("SkipEditableVoxel");
			}
			UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
			if (!SkipReason && (!Mesh || !Mesh->GetStaticMesh()))
			{
				++SkippedNonMesh; SkipReason = TEXT("SkipNotStaticMesh");
			}
			if (!SkipReason && !Mesh->IsQueryCollisionEnabled())
			{
				++SkippedNoQuery; SkipReason = TEXT("SkipNoQueryCollision");
			}
			if (SkipReason)
			{
				if (bNearby && (AbsorbOcclusionLogLevel >= 2 || FCString::Strcmp(SkipReason, TEXT("SkipNotStaticMesh")) != 0))
				{
					LogCandidate(Component, SkipReason, Bounds);
				}
				continue;
			}
			if (!bNearby) { continue; }
			LogCandidate(Component, TEXT("TestActualCollision"), Bounds);
			if (UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Mesh))
			{
				// 나무 사이의 합산 Bounds를 하나의 막힌 벽으로 취급하지 않는다.
				for (int32 Instance : Instanced->GetInstancesOverlappingBox(QueryBounds, true))
				{
					FTransform Transform;
					if (!Instanced->GetInstanceTransform(Instance, Transform, true)) { continue; }
					Builder.Obstacles.Add(Instanced->GetStaticMesh()->GetBoundingBox().TransformBy(Transform));
					Obstacles.Add({ Mesh, Mesh->GetBodyInstance(NAME_None, true, Instance), Instance });
				}
			}
			else
			{
				Builder.Obstacles.Add(Bounds);
				Obstacles.Add({ Mesh, Mesh->GetBodyInstance(), INDEX_NONE });
			}
		}
	}

	// 평면 Simple Collision은 복셀마다 정확한 유한 부피로 판정한다. 안전하게
	// 표현할 수 없는 Body만 비용이 큰 8x8 Hull 대체 경로로 보낸다.
	for (int32 Index = Obstacles.Num() - 1; Index >= 0; --Index)
	{
		const FObstacle& Obstacle = Obstacles[Index];
		const int32 FirstVolume = OutVolumes.Num();
		if (Obstacle.Body && DRSnowAbsorbPlanes::TrySnapshot(*Obstacle.Body, FrustumOrigin, OutVolumes))
		{
			if (AbsorbOcclusionLogLevel > 0)
			{
				for (int32 V = FirstVolume; V < OutVolumes.Num(); ++V)
				{
					UE_LOG(LogTemp, Log, TEXT("[DRSnowOcclusion][Planes] Id=%llu Component=%s Instance=%d Volume=%d Faces=%d InsetCm=%.3f Source=QueryGeometry"),
						QueryId, *Obstacle.Mesh->GetPathName(), Obstacle.Instance, V, OutVolumes[V].Planes.Num(), OutVolumes[V].InsetCm);
				}
			}
			Obstacles.RemoveAt(Index);
			Builder.Obstacles.RemoveAt(Index);
		}
		else if (AbsorbOcclusionLogLevel > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[DRSnowOcclusion][PlaneFallback] Id=%llu Component=%s Instance=%d Reason=NoValidFiniteQueryVolumeOrPacketBudget"),
				QueryId, *Obstacle.Mesh->GetPathName(), Obstacle.Instance);
		}
	}
	int32 PlaneCount = 0;
	for (const FDRSnowAbsorbConvex& V : OutVolumes) { PlaneCount += V.Planes.Num(); }
	int32 GeometryTests = 0;
	bool bGeometryBudgetExceeded = false;
	constexpr int32 MaxGeometryTests = 2048;
	Builder.IntersectsGeometry = [&](int32 Index, const Chaos::FConvex& Prefix)
	{
		FObstacle& Obstacle = Obstacles[Index];
		if (!Obstacle.Body || !Obstacle.Body->IsValidBodyInstance())
		{
			// Query Collision은 켜져 있지만 물리를 읽을 수 없는 장애물은 비어 있는 것이 아니라 미확인이다.
			++Obstacle.MissingBody;
			return true;
		}
		if (GeometryTests + 2 > MaxGeometryTests)
		{
			bGeometryBudgetExceeded = true;
			Builder.bBudgetExceeded = true;
			Builder.RemainingTests = 0;
			return true;
		}
		const FPhysicsGeometryCollection Geometry = FPhysicsInterface::GetGeometryCollection(Prefix);
		const FTransform Pose(FQuat::Identity, Builder.Origin);
		bool bHit = true; // 물리 Actor를 읽을 수 없더라도 경로를 열어서는 안 된다.
		const FBodyInstance* LockedBody = Obstacle.Body->WeldParent ? Obstacle.Body->WeldParent : Obstacle.Body;
		const bool bReadPhysics = FPhysicsCommand::ExecuteRead(LockedBody->GetPhysicsActor(), [&](const FPhysicsActorHandle&)
		{
			++GeometryTests;
			bHit = FPhysicsInterface::Overlap_Geom(Obstacle.Body, Geometry, Pose, nullptr, false);
			if (!bHit)
			{
				++GeometryTests;
				bHit = FPhysicsInterface::Overlap_Geom(Obstacle.Body, Geometry, Pose, nullptr, true);
			}
		});
		if (!bReadPhysics) { ++Obstacle.MissingBody; }
		if (bHit) { ++Obstacle.Hits; }
		else { ++Obstacle.Misses; }
		return bHit;
	};
	const double BuildStarted = AbsorbOcclusionLogLevel > 0 ? FPlatformTime::Seconds() : 0.;
	TArray<uint8> Depths = Builder.Build();
	const double BuildFinished = AbsorbOcclusionLogLevel > 0 ? FPlatformTime::Seconds() : 0.;
	int32 Blocked = 0, ClosedAtStart = 0;
	uint8 MinDepth = DRSnowAbsorbOcclusion::OpenDepth, MaxDepth = 0;
	for (uint8 Depth : Depths)
	{
		Blocked += Depth != DRSnowAbsorbOcclusion::OpenDepth;
		ClosedAtStart += Depth == 0;
		MinDepth = FMath::Min(MinDepth, Depth);
		MaxDepth = FMath::Max(MaxDepth, Depth);
	}
	if (AbsorbOcclusionLogLevel > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[DRSnowOcclusion] Id=%llu Mode=FinitePlanesV4 World=%s NetMode=%d Absorber=%s Origin=%s End=%s Scanned=%d FallbackObstacles=%d SkipVoxel=%d SkipOwner=%d SkipNoQuery=%d SkipNonMesh=%d BoundsTests=%d GeometryTests=%d BudgetExceeded=%d GeometryBudgetExceeded=%d FallbackBlockedCells=%d/64 ClosedAtStart=%d MinDepth=%d MaxDepth=%d SurfaceAllowanceCm=%.1f Hulls=%d InvalidHulls=%d GatherMs=%.3f BuildMs=%.3f DetailSuppressed=%d PlaneVolumes=%d PlaneCount=%d"),
			QueryId, *World.GetName(), static_cast<int32>(World.GetNetMode()), *Owner.GetPathName(),
			*FrustumOrigin.ToCompactString(), *FrustumEnd.ToCompactString(), Scanned, Obstacles.Num(),
			SkippedVoxel, SkippedOwner, SkippedNoQuery, SkippedNonMesh, Builder.Tests, GeometryTests,
			Builder.bBudgetExceeded, bGeometryBudgetExceeded, Blocked, ClosedAtStart, MinDepth, MaxDepth,
			DRSnowAbsorbPlanes::SurfaceAbsorbAllowanceCm,
			Builder.HullsBuilt, Builder.InvalidHulls, (BuildStarted - GatherStarted) * 1000.0, (BuildFinished - BuildStarted) * 1000.0,
			DetailSuppressed, OutVolumes.Num(), PlaneCount);
		for (int32 Index = 0; Index < Obstacles.Num(); ++Index)
		{
			const FObstacle& Obstacle = Obstacles[Index];
			if (AbsorbOcclusionLogLevel < 2 && Index >= 24) { break; }
			UE_LOG(LogTemp, Log,
				TEXT("[DRSnowOcclusion][Geometry] Id=%llu Component=%s Instance=%d Hits=%d BoundsOnlyMisses=%d MissingBody=%d"),
				QueryId, *Obstacle.Mesh->GetPathName(), Obstacle.Instance, Obstacle.Hits, Obstacle.Misses, Obstacle.MissingBody);
		}
		if (AbsorbOcclusionLogLevel >= 2)
		{
			for (int32 Y = 0; Y < DRSnowAbsorbOcclusion::Resolution; ++Y)
			{
				FString Row;
				for (int32 X = 0; X < DRSnowAbsorbOcclusion::Resolution; ++X)
				{
					const uint8 Depth = Depths[DRSnowAbsorbOcclusion::GetCellIndex(X, Y)];
					Row += Depth == DRSnowAbsorbOcclusion::OpenDepth ? TEXT("Open ")
						: FString::Printf(TEXT("%.1f "), Builder.Range * Depth / DRSnowAbsorbOcclusion::MaxBlockedDepth);
				}
				UE_LOG(LogTemp, Log, TEXT("[DRSnowOcclusion][DepthCm] Id=%llu Row=%d Values=%s"), QueryId, Y, *Row);
			}
		}
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Occlusion/BoundsTests"), Builder.Tests);
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Occlusion/GeometryTests"), GeometryTests);
#if ENABLE_DRAW_DEBUG
	const IConsoleVariable* OcclusionDrawVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("dr.Snow.Absorb.DebugDraw"));
	if (OcclusionDrawVariable && OcclusionDrawVariable->GetInt() >= 2)
	{
		// 이전 타일 전체 Cutoff가 아니라 위치별 평면 판정 결과를 표시한다.
		// 샘플링은 디버그 전용이며, 실제 복셀 판정은 해석적으로 처리한다.
		for (int32 Y = 0; Y < DRSnowAbsorbOcclusion::Resolution; ++Y)
		for (int32 X = 0; X < DRSnowAbsorbOcclusion::Resolution; ++X)
		{
			const uint8 Encoded = Depths[DRSnowAbsorbOcclusion::GetCellIndex(X, Y)];
			const float Safe = Encoded == DRSnowAbsorbOcclusion::OpenDepth ? Builder.Range
				: Builder.Range * Encoded / DRSnowAbsorbOcclusion::MaxBlockedDepth;
			const FVector Radial = Builder.AxisY * (2.f * (X + 0.5f) / DRSnowAbsorbOcclusion::Resolution - 1.f)
				+ Builder.AxisZ * (2.f * (Y + 0.5f) / DRSnowAbsorbOcclusion::Resolution - 1.f);
			const auto DeltaAt = [&](float D)
			{
				return Builder.Direction * D + Radial * FMath::Lerp(Builder.InnerRadius, Builder.OuterRadius, D / Builder.Range);
			};
			const auto BlockedAt = [&](float D)
			{
				return (Encoded != DRSnowAbsorbOcclusion::OpenDepth && D >= Safe) ||
					DRSnowAbsorbPlanes::IsBlocked(OutVolumes, DeltaAt(D));
			};
			float RunStart = 0.f, PreviousD = 0.f;
			bool bPreviousBlocked = BlockedAt(0.f);
			for (int32 Step = 1; Step <= 64; ++Step)
			{
				const float D = Builder.Range * Step / 64.f;
				const bool bBlocked = BlockedAt(D);
				if (bBlocked != bPreviousBlocked)
				{
					float Low = PreviousD, High = D;
					for (int32 Refine = 0; Refine < 10; ++Refine)
					{
						const float Mid = (Low + High) * 0.5f;
						if (BlockedAt(Mid) == bPreviousBlocked) { Low = Mid; } else { High = Mid; }
					}
					DrawDebugLine(&World, Builder.Origin + DeltaAt(RunStart), Builder.Origin + DeltaAt(High),
						bPreviousBlocked ? FColor::Red : FColor::Green, false, 0.2f, 0, 1.f);
					DrawDebugPoint(&World, Builder.Origin + DeltaAt(High), 4.f, FColor::Yellow, false, 0.2f);
					RunStart = High;
					bPreviousBlocked = bBlocked;
				}
				PreviousD = D;
			}
			DrawDebugLine(&World, Builder.Origin + DeltaAt(RunStart), Builder.Origin + DeltaAt(Builder.Range),
				bPreviousBlocked ? FColor::Red : FColor::Green, false, 0.2f, 0, 1.f);
		}
	}
#endif
	if (Blocked == 0) { Depths.Reset(); }
	return Depths;
}
}

static TAutoConsoleVariable<int32> CVarDrawSnowAbsorbDebug(
	TEXT("dr.Snow.DrawAbsorbDebug"),
	0,
	TEXT("눈 흡수 디버그 형상을 그린다.\n")
	TEXT("0: 끔\n")
	TEXT("1: 켬"),
	ECVF_Cheat);

UDRSnowRemoveComponent::UDRSnowRemoveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UDRSnowRemoveComponent::TryRemoveSnowFromHit(const FHitResult& HitResult, const FDRSnowRemovalSpec& RemovalSpec)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowFromHit(HitResult, RemovalSpec);
		return 0.f;
	}

	if (!HitResult.bBlockingHit)
	{
		return 0.f;
	}

	if (!CanRemoveNow(RemovalSpec))
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	FDRSnowSurfaceRemoveRequest Request =
		MakeRemoveRequest(HitResult.ImpactPoint, HitResult.ImpactNormal, HitResult.TraceStart, RemovalSpec);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);
	return ExecuteRemoveRequest(Request, GetInteractableActorFromHit(HitResult));
}

float UDRSnowRemoveComponent::TryRemoveSnowAtLocation(
	FVector WorldLocation,
	FVector SurfaceNormal,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowAtLocation(WorldLocation, SurfaceNormal.GetSafeNormal(), RemovalSpec);
		return 0.f;
	}

	if (!CanRemoveNow(RemovalSpec))
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	const FVector BrushOrigin = IsValid(Owner) ? Owner->GetActorLocation() : WorldLocation;
	const FDRSnowSurfaceRemoveRequest Request =
		MakeRemoveRequest(WorldLocation, SurfaceNormal, BrushOrigin, RemovalSpec);

	return ExecuteRemoveRequest(Request);
}

float UDRSnowRemoveComponent::TryRemoveSnowAlongDirection(
	FVector BrushOrigin,
	FVector Direction,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !CanRemoveNow(RemovalSpec))
	{
		return 0.f;
	}

	const FVector NormalizedDirection = Direction.GetSafeNormal();
	if (NormalizedDirection.IsNearlyZero() || RemovalSpec.SnowAbsorbRange <= 0.f)
	{
		return 0.f;
	}

	LastRemoveTime = GetWorld()->GetTimeSeconds();

	// BrushOrigin은 호출자가 정한 서버 안전 Gameplay 기준점이며,
	// 여기서 캐릭터 로컬 StartOffset을 추가해 실제 Absorb Frustum 시작점을 계산한다.
	const FVector WorldStartOffset = Owner->GetActorTransform().TransformVectorNoScale(
		RemovalSpec.SnowAbsorbStartOffset);
	// 제거 작업은 FVector_NetQuantize 끝점을 전송한다. 큰 충돌 여유로 서버와
	// 클라이언트의 좌표 오차를 감추지 않고, Mask와 편집을 같은 정수 좌표에서 처리한다.
	const auto CanonicalPosition = [](const FVector& Position)
	{
		return FVector(FMath::RoundToDouble(Position.X), FMath::RoundToDouble(Position.Y), FMath::RoundToDouble(Position.Z));
	};
	const FVector FrustumOrigin = CanonicalPosition(BrushOrigin + WorldStartOffset);
	const FVector FrustumEnd = CanonicalPosition(FrustumOrigin + NormalizedDirection * RemovalSpec.SnowAbsorbRange);
	const FVector AbsorbDirection = (FrustumEnd - FrustumOrigin).GetSafeNormal();
	
#if ENABLE_DRAW_DEBUG
	if (CVarDrawSnowAbsorbDebug.GetValueOnGameThread() != 0)
	{
		const float EndRadius = RemovalSpec.SnowAbsorbRadius;
		const float StartRadius = EndRadius * RemovalSpec.SnowAbsorbInnerRadiusRatio;

		FVector AxisY;
		FVector AxisZ;
		AbsorbDirection.FindBestAxisVectors(AxisY, AxisZ);

		DrawDebugLine(GetWorld(), FrustumOrigin, FrustumEnd, FColor::Cyan, false, 0.15f, 0, 2.f);
		DrawDebugCircle(GetWorld(), FrustumOrigin, StartRadius, 24, FColor::Green, false, 0.15f, 0, 2.f, AxisY, AxisZ, false);
		DrawDebugCircle(GetWorld(), FrustumEnd, EndRadius, 24, FColor::Red, false, 0.15f, 0, 2.f, AxisY, AxisZ, false);
		DrawDebugLine(GetWorld(), FrustumOrigin + AxisY * StartRadius, FrustumEnd + AxisY * EndRadius, FColor::Yellow, false, 0.15f);
		DrawDebugLine(GetWorld(), FrustumOrigin - AxisY * StartRadius, FrustumEnd - AxisY * EndRadius, FColor::Yellow, false, 0.15f);
		DrawDebugLine(GetWorld(), FrustumOrigin + AxisZ * StartRadius, FrustumEnd + AxisZ * EndRadius, FColor::Yellow, false, 0.15f);
		DrawDebugLine(GetWorld(), FrustumOrigin - AxisZ * StartRadius, FrustumEnd - AxisZ * EndRadius, FColor::Yellow, false, 0.15f);
	}
#endif

	FDRSnowSurfaceRemoveRequest Request = MakeRemoveRequest(
		FrustumEnd,
		-AbsorbDirection,
		FrustumOrigin,
		RemovalSpec);
	Request.AbsorbOcclusionDepths = BuildAbsorbOcclusionDepths(
		*GetWorld(),
		*Owner,
		FrustumOrigin,
		FrustumEnd,
		RemovalSpec.SnowAbsorbRadius * FMath::Clamp(RemovalSpec.SnowAbsorbInnerRadiusRatio, 0.f, 1.f),
		RemovalSpec.SnowAbsorbRadius,
		Request.AbsorbOcclusionVolumes);
	return ExecuteRemoveRequest(Request, nullptr, true);
}

FDRSnowSurfaceRemoveRequest UDRSnowRemoveComponent::MakeRemoveRequest(
	FVector WorldLocation,
	FVector SurfaceNormal,
	FVector BrushOrigin,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	FDRSnowSurfaceRemoveRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	Request.BrushOrigin = BrushOrigin;
	Request.Radius = FMath::Max(0.f, RemovalSpec.SnowAbsorbRadius);
	Request.RequestedAmount = FMath::Max(0.f, RemovalSpec.SnowAbsorbPower);
	Request.RemovalBrushShape = RemovalSpec.RemovalBrushShape;
	Request.RemovalMode = RemovalSpec.RemovalMode;
	Request.AbsorbInnerRadiusRatio = FMath::Clamp(RemovalSpec.SnowAbsorbInnerRadiusRatio, 0.f, 1.f);
	Request.AbsorbSweepRadius = FMath::Max(1.f, RemovalSpec.SnowAbsorbSweepRadius);
	Request.AbsorbMaxSweepsPerTick = FMath::Max(1, RemovalSpec.SnowAbsorbMaxSweepsPerTick);
	Request.bUseAdaptiveAbsorbQuery = RemovalSpec.bUseAdaptiveAbsorbQuery;
	Request.Context = MakeInteractionContext();
	return Request;
}

float UDRSnowRemoveComponent::ExecuteRemoveRequest(
	const FDRSnowSurfaceRemoveRequest& Request,
	AActor* FallbackTarget,
	const bool bUseAbsorbTool)
{
	float RemovedAmount = 0.f;
	if (UWorld* World = GetWorld())
	{
		if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
		{
			const FDRSnowRemoveResult RemoveResult = bUseAbsorbTool
				? SnowSubsystem->RemoveSnowWithAbsorbTool(Request)
				: SnowSubsystem->RemoveSnow(Request);
			RemovedAmount = RemoveResult.RemovedAmount;
			if (RemovedAmount > 0.f)
			{
				if (ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>())
				{
					FDRSnowRemoveOperation Operation;
					Operation.WorldLocation = Request.WorldLocation;
					Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
					Operation.BrushOrigin = Request.BrushOrigin;
					Operation.Radius = Request.Radius;
					Operation.RequestedAmount = Request.RequestedAmount;
					Operation.AppliedAmount = RemovedAmount;
					Operation.RemovalBrushShape = Request.RemovalBrushShape;
					Operation.RemovalMode = Request.RemovalMode;
					Operation.AbsorbInnerRadiusRatio = Request.AbsorbInnerRadiusRatio;
					Operation.AbsorbSweepRadius = Request.AbsorbSweepRadius;
					Operation.AbsorbMaxSweepsPerTick = Request.AbsorbMaxSweepsPerTick;
					Operation.bUseAdaptiveAbsorbQuery = Request.bUseAdaptiveAbsorbQuery;
					Operation.AbsorbOcclusionDepths = Request.AbsorbOcclusionDepths;
					Operation.AbsorbOcclusionVolumes = Request.AbsorbOcclusionVolumes;
					Operation.TeamId = Request.Context.TeamId;
					Operation.VoxelWorldName =
						IsValid(Request.TargetVoxelWorld.Get())
							? Request.TargetVoxelWorld->GetFName()
							: NAME_None;
					MiningGameState->RegisterSnowRemove(Operation, RemoveResult.EditedWorldBounds);
				}
			}
		}
	}

	if (RemovedAmount <= 0.f &&
		IsValid(FallbackTarget) &&
		FallbackTarget->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowRemove(FallbackTarget, Request))
	{
		RemovedAmount = IDRSnowInteractableInterface::Execute_ReceiveSnowRemoved(FallbackTarget, Request);
	}

	OnSnowRemoved.Broadcast(Request, RemovedAmount);
	return FMath::Max(0.f, RemovedAmount);
}

bool UDRSnowRemoveComponent::CanRemoveNow(const FDRSnowRemovalSpec& RemovalSpec) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const float AbsorbSpeed = FMath::Max(0.f, RemovalSpec.SnowAbsorbSpeed);
	if (AbsorbSpeed <= UE_SMALL_NUMBER)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastRemoveTime >= 1.f / AbsorbSpeed;
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowFromHit_Implementation(
	const FHitResult& HitResult,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	TryRemoveSnowFromHit(HitResult, RemovalSpec);
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowAtLocation_Implementation(
	FVector_NetQuantize WorldLocation,
	FVector_NetQuantizeNormal SurfaceNormal,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	TryRemoveSnowAtLocation(WorldLocation, SurfaceNormal, RemovalSpec);
}

AVoxelWorld* UDRSnowRemoveComponent::GetVoxelWorldFromHit(const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent) ? Cast<AVoxelWorld>(HitComponent->GetOwner()) : nullptr;
}
