#include "DRSnowAbsorbTool.h"
#include "DRSnowSurfaceQuery.h"

#include "DrawDebugHelpers.h"
#include "DRSnowTypes.h"
#include "DRSnowAbsorbPlanes.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelMaterial.h"
#include "VoxelWorld.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogDRSnowAbsorb, Log, All);

TAutoConsoleVariable<int32> CVarDRSnowAbsorbDebugDraw(
	TEXT("dr.Snow.Absorb.DebugDraw"),
	0,
	TEXT("눈 흡수 디버그 선을 그린다. 0: 끔, 1: 원본 범위, 2: 서버 차폐 경로도 표시(초록 허용, 빨강 차폐, 노랑 경계)"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarDRSnowAbsorbLog(
	TEXT("dr.Snow.Absorb.Log"),
	0,
	TEXT("눈 흡수 파이프라인 로그를 남긴다. 0: 끔, 1: 요약 및 주변 차폐물, 2: 전체 후보·깊이 격자·복셀 샘플"),
	ECVF_Default);

float SmoothStep(const float Value)
{
	const float ClampedValue = FMath::Clamp(Value, 0.f, 1.f);
	return ClampedValue * ClampedValue * (3.f - 2.f * ClampedValue);
}

// Voxel 밀도는 0 미만일 때만 고체(눈)다. 빈 공간(0 이상)의 수치 보정은
// 눈을 얻은 것이 아니므로 게이지용 제거량에 포함하지 않는다.
float GetRemovedSolidDensityAmount(const FModifiedVoxelValue& ModifiedValue)
{
	const float OldSolidDensity = FMath::Min(ModifiedValue.OldValue, 0.f);
	const float NewSolidDensity = FMath::Min(ModifiedValue.NewValue, 0.f);
	return FMath::Max(0.f, NewSolidDensity - OldSolidDensity);
}

bool IsBehindAbsorbOccluder(
	const TArray<uint8>& OcclusionDepths,
	const TArray<FDRSnowAbsorbConvex>& OcclusionVolumes,
	const float SurfaceAllowanceCm,
	const FVector& PointDelta,
	const FVector& RadialDelta,
	const FVector& AxisY,
	const FVector& AxisZ,
	const float RadiusAtPoint,
	const float DistanceAlong,
	const float FrustumRange)
{
	if (DRSnowAbsorbPlanes::IsBlocked(OcclusionVolumes, PointDelta, SurfaceAllowanceCm)) { return true; }
	if (OcclusionDepths.IsEmpty()) { return false; }
	if (OcclusionDepths.Num() != DRSnowAbsorbOcclusion::SampleCount ||
		RadiusAtPoint <= KINDA_SMALL_NUMBER || FrustumRange <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float NormalizedX = FVector::DotProduct(RadialDelta, AxisY) / RadiusAtPoint;
	const float NormalizedY = FVector::DotProduct(RadialDelta, AxisZ) / RadiusAtPoint;
	// 타일에는 샘플 레이 깊이가 아니라 검증된 빈 부피의 접두 구간을 저장한다.
	// 보간하면 검증되지 않았거나 차폐된 영역까지 접두 구간이 늘어나므로 사용하지 않는다.
	const int32 X = FMath::Clamp(FMath::FloorToInt(
		(NormalizedX + 1.f) * 0.5f * DRSnowAbsorbOcclusion::Resolution),
		0, DRSnowAbsorbOcclusion::Resolution - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(
		(NormalizedY + 1.f) * 0.5f * DRSnowAbsorbOcclusion::Resolution),
		0, DRSnowAbsorbOcclusion::Resolution - 1);
	const uint8 Depth = OcclusionDepths[DRSnowAbsorbOcclusion::GetCellIndex(X, Y)];
	if (Depth == DRSnowAbsorbOcclusion::OpenDepth) { return false; }
	const float SafeDepth = FrustumRange * Depth / DRSnowAbsorbOcclusion::MaxBlockedDepth;
	// 복잡한 StaticMesh 충돌은 평면 스냅샷 대신 보수적 깊이 마스크를 사용한다.
	// 이 경로도 표면 안쪽 1cm는 허용해 바닥에 붙은 얇은 눈이 남지 않게 한다.
	return DistanceAlong >= SafeDepth + SurfaceAllowanceCm;
}

// 밀도 편집 뒤, 첫 렌더 갱신 전에 머티리얼 데이터를 복구한다.
// FVoxelSurfaceEditToolsImpl::PropagateVoxelMaterials와 달리 표면 샘플에
// 채워진 이웃이 없어도 assert하지 않고 해당 샘플을 그대로 둔다. 실제로
// 밀도가 바뀐 복셀만 처리한다.
void RepairAbsorbMaterialsBeforeRender(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	const FVoxelIntBox& EditedBounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_RepairMaterials);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB ||
		!ProcessedVoxels.Info.bHasSurfacePositions ||
		ProcessedVoxels.Voxels->IsEmpty() || ModifiedValues.IsEmpty() ||
		!EditedBounds.IsValid())
	{
		return;
	}

	TMap<FIntVector, const FVoxelSurfaceEditsVoxel*> SurfaceVoxelByPosition;
	SurfaceVoxelByPosition.Reserve(ProcessedVoxels.Voxels->Num());
	for (const FVoxelSurfaceEditsVoxel& SurfaceVoxel : *ProcessedVoxels.Voxels)
	{
		SurfaceVoxelByPosition.Add(SurfaceVoxel.Position, &SurfaceVoxel);
	}

	FVoxelData& Data = VoxelWorld->GetData();
	const FVoxelIntBox LockBounds = ProcessedVoxels.Bounds.Extend(1);
	int32 RepairedCount = 0;
	int32 NoSourceCount = 0;
	{
		FVoxelWriteScopeLock Lock(Data, LockBounds, FUNCTION_FNAME);
		for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
		{
			if (ModifiedValue.NewValue <= ModifiedValue.OldValue)
			{
				continue;
			}

			const FVoxelSurfaceEditsVoxel* const* FoundSurfaceVoxel =
				SurfaceVoxelByPosition.Find(ModifiedValue.Position);
			if (!FoundSurfaceVoxel || !*FoundSurfaceVoxel)
			{
				++NoSourceCount;
				continue;
			}

			const FVoxelSurfaceEditsVoxel& SurfaceVoxel = **FoundSurfaceVoxel;
			const FVector SurfaceWorldLocation =
				VoxelWorld->LocalToGlobalFloat(FVoxelVector(SurfaceVoxel.SurfacePosition));
			const TArray<FIntVector> Neighbors =
				VoxelWorld->GetNeighboringPositions(SurfaceWorldLocation);

			bool bFoundFilledNeighbor = false;
			double ClosestDistanceSquared = TNumericLimits<double>::Max();
			FIntVector ClosestPosition(ForceInit);
			for (const FIntVector& Neighbor : Neighbors)
			{
				const FVoxelValue NeighborValue = Data.GetValue(Neighbor, 0);
				if (NeighborValue.IsEmpty())
				{
					continue;
				}

				const double DistanceSquared = static_cast<double>(
					(FVoxelVector(Neighbor) - SurfaceVoxel.SurfacePosition).SizeSquared());
				if (!bFoundFilledNeighbor || DistanceSquared < ClosestDistanceSquared)
				{
					bFoundFilledNeighbor = true;
					ClosestDistanceSquared = DistanceSquared;
					ClosestPosition = Neighbor;
				}
			}

			if (!bFoundFilledNeighbor)
			{
				++NoSourceCount;
				continue;
			}

			Data.SetMaterial(ModifiedValue.Position, Data.GetMaterial(ClosestPosition, 0));
			++RepairedCount;
		}
	}

	if (CVarDRSnowAbsorbLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogDRSnowAbsorb, Log,
			TEXT("Material repair: Modified=%d Repaired=%d NoFilledNeighbor=%d"),
			ModifiedValues.Num(), RepairedCount, NoSourceCount);
	}
}

void DrawAbsorbRange(
	UWorld* World,
	const FVector& BrushOrigin,
	const FVector& TargetLocation,
	const float InnerRadius,
	const float OuterRadius)
{
	if (!IsValid(World) || CVarDRSnowAbsorbDebugDraw.GetValueOnGameThread() == 0)
	{
		return;
	}

	const FVector Axis = (TargetLocation - BrushOrigin).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return;
	}
	const FVector ReferenceUp = FMath::Abs(Axis.Z) > 0.99f ? FVector::ForwardVector : FVector::UpVector;
	const FVector Right = FVector::CrossProduct(ReferenceUp, Axis).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Axis, Right).GetSafeNormal();
	constexpr int32 SideCount = 24;
	for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
	{
		const float AngleA = 2.f * PI * SideIndex / SideCount;
		const float AngleB = 2.f * PI * (SideIndex + 1) / SideCount;
		const FVector RingDirectionA = Right * FMath::Cos(AngleA) + Up * FMath::Sin(AngleA);
		const FVector RingDirectionB = Right * FMath::Cos(AngleB) + Up * FMath::Sin(AngleB);
		const FVector NearA = BrushOrigin + RingDirectionA * InnerRadius;
		const FVector NearB = BrushOrigin + RingDirectionB * InnerRadius;
		const FVector FarA = TargetLocation + RingDirectionA * OuterRadius;
		const FVector FarB = TargetLocation + RingDirectionB * OuterRadius;
		DrawDebugLine(World, NearA, NearB, FColor::Yellow, false, 0.2f, 0, 1.5f);
		DrawDebugLine(World, FarA, FarB, FColor::Cyan, false, 0.2f, 0, 1.5f);
		DrawDebugLine(World, NearA, FarA, FColor::Orange, false, 0.2f, 0, 1.5f);
	}
}
}

UDRSnowAbsorbTool::UDRSnowAbsorbTool()
{
	ToolName = TEXT("DR Snow Absorb Tool");
}

void UDRSnowAbsorbTool::GetToolConfig(FVoxelToolBaseConfig& OutConfig) const
{
	OutConfig.bHasAlignment = false;
}

FVoxelIntBoxWithValidity UDRSnowAbsorbTool::DoEdit()
{
	AVoxelWorld* World = GetVoxelWorld();
	if (!IsValid(World) || !World->IsCreated() || !SharedConfig)
	{
		return {};
	}

	const float Length = SharedConfig->BrushSize;
	const FVector Origin = GetToolPosition();
	const FVector Target = Origin + GetToolDirection().GetSafeNormal() * Length;
	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelIntBox EditedBounds;
	RemoveSnowFromFrustum(
		World,
		Origin,
		Target,
		Length * 0.5f,
		InnerRadiusRatio,
		FarStrengthRatio,
		Strength,
		DistanceDivisor,
		TArray<uint8>(),
		TArray<FDRSnowAbsorbConvex>(),
		ModifiedValues,
		EditedBounds,
		false);

	return EditedBounds.IsValid() ? FVoxelIntBoxWithValidity(EditedBounds) : FVoxelIntBoxWithValidity();
}

float UDRSnowAbsorbTool::RemoveSnowFromFrustum(
	AVoxelWorld* VoxelWorld,
	const FVector& BrushOrigin,
	const FVector& TargetLocation,
	const float OuterRadius,
	const float InnerRadiusRatio,
	const float FarStrengthRatio,
	const float Strength,
	const float DistanceDivisor,
	const TArray<uint8>& OcclusionDepths,
	const TArray<FDRSnowAbsorbConvex>& OcclusionVolumes,
	TArray<FModifiedVoxelValue>& OutModifiedValues,
	FVoxelIntBox& OutEditedBounds,
	const bool bUpdateRender)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Full_Total);
	OutModifiedValues.Reset();
	OutEditedBounds = FVoxelIntBox();
	const bool bLogAbsorb = CVarDRSnowAbsorbLog.GetValueOnGameThread() != 0;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || OuterRadius <= 0.f ||
		Strength <= 0.f || DistanceDivisor <= 0.f)
	{
		if (bLogAbsorb)
		{
			UE_LOG(LogDRSnowAbsorb, Warning, TEXT("Rejected input: World=%s Created=%d Radius=%.2f Strength=%.2f Divisor=%.2f"),
				*GetNameSafe(VoxelWorld), IsValid(VoxelWorld) && VoxelWorld->IsCreated(), OuterRadius, Strength, DistanceDivisor);
		}
		return 0.f;
	}

	const FVector Segment = TargetLocation - BrushOrigin;
	const float Length = Segment.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		if (bLogAbsorb)
		{
			UE_LOG(LogDRSnowAbsorb, Warning, TEXT("Rejected zero-length frustum: Origin=%s Target=%s"),
				*BrushOrigin.ToString(), *TargetLocation.ToString());
		}
		return 0.f;
	}
	if (bLogAbsorb)
	{
		UE_LOG(LogDRSnowAbsorb, Log, TEXT("Begin: World=%s Origin=%s Target=%s Range=%.1f Radius=%.1f Inner=%.2f Strength=%.2f Divisor=%.2f"),
			*GetNameSafe(VoxelWorld), *BrushOrigin.ToString(), *TargetLocation.ToString(), Length, OuterRadius, InnerRadiusRatio, Strength, DistanceDivisor);
	}
	DrawAbsorbRange(
		VoxelWorld->GetWorld(),
		BrushOrigin,
		TargetLocation,
		OuterRadius * FMath::Clamp(InnerRadiusRatio, 0.f, 1.f),
		OuterRadius);

	const FVector Direction = Segment / Length;
	// 구간 전체를 감싸는 구 반경을 넘기면 1,000cm 흡수 시 1,200cm 정육면체를 매 틱 탐색한다.
	// 시작/끝점만 포함하는 축 정렬 범위로 좁혀 표면 탐색과 렌더 갱신 비용을 제한한다.
	const FVector BoundsPadding(OuterRadius);
	const FBox GlobalBounds(
		FVector(
			FMath::Min(BrushOrigin.X, TargetLocation.X) - BoundsPadding.X,
			FMath::Min(BrushOrigin.Y, TargetLocation.Y) - BoundsPadding.Y,
			FMath::Min(BrushOrigin.Z, TargetLocation.Z) - BoundsPadding.Z),
		FVector(
			FMath::Max(BrushOrigin.X, TargetLocation.X) + BoundsPadding.X,
			FMath::Max(BrushOrigin.Y, TargetLocation.Y) + BoundsPadding.Y,
			FMath::Max(BrushOrigin.Z, TargetLocation.Z) + BoundsPadding.Z));
	FBox LocalBounds(ForceInit);
	for (int32 X = 0; X < 2; ++X)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 Z = 0; Z < 2; ++Z)
			{
				const FVector GlobalCorner(
					X == 0 ? GlobalBounds.Min.X : GlobalBounds.Max.X,
					Y == 0 ? GlobalBounds.Min.Y : GlobalBounds.Max.Y,
					Z == 0 ? GlobalBounds.Min.Z : GlobalBounds.Max.Z);
				LocalBounds += VoxelWorld->GlobalToLocalFloat(GlobalCorner).ToFloat();
			}
		}
	}
	const FVoxelIntBox Bounds(LocalBounds);
	if (!Bounds.IsValid())
	{
		if (bLogAbsorb)
		{
			UE_LOG(LogDRSnowAbsorb, Warning, TEXT("Rejected invalid local bounds: GlobalMin=%s GlobalMax=%s"),
				*GlobalBounds.Min.ToString(), *GlobalBounds.Max.ToString());
		}
		return 0.f;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Full_QuerySurface);
		DRSnowSurfaceQuery::FindSurface(SurfaceVoxels, VoxelWorld, Bounds);
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Full/SurfaceQueried"), SurfaceVoxels.Voxels->Num());

	const float InnerRadius = OuterRadius * FMath::Clamp(InnerRadiusRatio, 0.f, 1.f);
	const float ClampedFarStrengthRatio = FMath::Clamp(FarStrengthRatio, 0.f, 1.f);
	// 밀도 표면은 충돌체 안쪽으로 최대 반 복셀까지 위치할 수 있다. 1cm 고정값만
	// 허용하면 표면에 끼인 눈 한 층이 계속 차폐되므로, 복셀 해상도에 맞춘다.
	const float SurfaceAllowanceCm = FMath::Max(
		DRSnowAbsorbPlanes::SurfaceAbsorbAllowanceCm,
		VoxelWorld->VoxelSize * 0.5f);
	FVector OcclusionAxisY;
	FVector OcclusionAxisZ;
	Direction.FindBestAxisVectors(OcclusionAxisY, OcclusionAxisZ);
	TArray<FVoxelSurfaceEditsVoxel> AbsorbVoxels;
	int32 FrustumVoxelCount = 0;
	int32 StrengthVoxelCount = 0;
	for (const FVoxelSurfaceEditsVoxelBase& SourceVoxel : *SurfaceVoxels.Voxels)
	{
		const FVector VoxelLocation = VoxelWorld->LocalToGlobal(SourceVoxel.Position);
		const FVector Delta = VoxelLocation - BrushOrigin;
		const float DistanceAlong = FVector::DotProduct(Delta, Direction);
		if (DistanceAlong < 0.f || DistanceAlong > Length)
		{
			continue;
		}

		const float AlongAlpha = DistanceAlong / Length;
		const float RadiusAtPoint = FMath::Lerp(InnerRadius, OuterRadius, AlongAlpha);
		const FVector RadialDelta = Delta - Direction * DistanceAlong;
		const float RadialAlpha = RadiusAtPoint > KINDA_SMALL_NUMBER
			? RadialDelta.Size() / RadiusAtPoint
			: (RadialDelta.IsNearlyZero() ? 0.f : BIG_NUMBER);
		if (RadialAlpha > 1.f)
		{
			continue;
		}
		if (IsBehindAbsorbOccluder(
			OcclusionDepths,
			OcclusionVolumes,
			SurfaceAllowanceCm,
			Delta,
			RadialDelta,
			OcclusionAxisY,
			OcclusionAxisZ,
			RadiusAtPoint,
			DistanceAlong,
			Length))
		{
			continue;
		}
		++FrustumVoxelCount;

		// 시작점(흡수구)으로 갈수록 강해지고, 외곽은 자연스럽게 약해진다.
		const float DistanceWeight = FMath::Lerp(1.f, ClampedFarStrengthRatio, SmoothStep(AlongAlpha));
		const float RadialWeight = 1.f - SmoothStep(RadialAlpha);
		const float VoxelStrength = Strength * DistanceWeight * RadialWeight;
		if (VoxelStrength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		++StrengthVoxelCount;

		FVoxelSurfaceEditsVoxel& AbsorbVoxel = AbsorbVoxels.Add_GetRef(FVoxelSurfaceEditsVoxel(SourceVoxel));
		AbsorbVoxel.Strength = VoxelStrength;
	}

	if (bLogAbsorb)
	{
		UE_LOG(LogDRSnowAbsorb, Log, TEXT("Surface query: Surface=%d InFrustum=%d PositiveStrength=%d EditVoxels=%d Bounds=[%s -> %s]"),
			SurfaceVoxels.Voxels->Num(), FrustumVoxelCount, StrengthVoxelCount, AbsorbVoxels.Num(),
			*Bounds.Min.ToString(), *Bounds.Max.ToString());
	}
	if (AbsorbVoxels.IsEmpty())
	{
		if (bLogAbsorb)
		{
			UE_LOG(LogDRSnowAbsorb, Warning, TEXT("No editable voxel remained after frustum and strength filtering"));
		}
		return 0.f;
	}

	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Full/Candidates"), AbsorbVoxels.Num());
	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
	ProcessedVoxels.Bounds = Bounds;
	ProcessedVoxels.Info.bHasValues = true;
	ProcessedVoxels.Info.bHasNormals = true;
	ProcessedVoxels.Info.bHasSurfacePositions = true;
	ProcessedVoxels.Info.bHasExactDistanceField = true;
	ProcessedVoxels.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(AbsorbVoxels));

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_EditValues);
		// 머티리얼 데이터 복구가 끝날 때까지 렌더 갱신을 보류한다. 렌더러가
		// 중간 밀도 전용 상태를 보지 않게 한다.
		UVoxelSurfaceEditTools::EditVoxelValues(
			OutModifiedValues,
			OutEditedBounds,
			VoxelWorld,
			ProcessedVoxels,
			DistanceDivisor,
			true,
			true,
			false);
	}
	RepairAbsorbMaterialsBeforeRender(
		VoxelWorld, ProcessedVoxels, OutModifiedValues, OutEditedBounds);
	if (bUpdateRender && OutEditedBounds.IsValid())
	{
		FVoxelToolHelpers::UpdateWorld(VoxelWorld, OutEditedBounds);
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Full/ModifiedValues"), OutModifiedValues.Num());

	float ModifiedAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : OutModifiedValues)
	{
		ModifiedAmount += GetRemovedSolidDensityAmount(ModifiedValue);
	}
	if (bLogAbsorb)
	{
		UE_LOG(LogDRSnowAbsorb, Log, TEXT("Edit result: ModifiedValues=%d EditedBoundsValid=%d Applied=%.4f"),
			OutModifiedValues.Num(), OutEditedBounds.IsValid(), ModifiedAmount);
	}
	return ModifiedAmount;
}

float UDRSnowAbsorbTool::RemoveSnowFromFrustumAdaptive(
	AVoxelWorld* VoxelWorld,
	const FVector& BrushOrigin,
	const FVector& TargetLocation,
	const float OuterRadius,
	const float InnerRadiusRatio,
	const float FarStrengthRatio,
	const float Strength,
	const float DistanceDivisor,
	const float SweepRadius,
	const int32 MaxSweepsPerTick,
	const TArray<uint8>& OcclusionDepths,
	const TArray<FDRSnowAbsorbConvex>& OcclusionVolumes,
	TArray<FModifiedVoxelValue>& OutModifiedValues,
	FVoxelIntBox& OutEditedBounds,
	const bool bUpdateRender)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Adaptive_Total);
	OutModifiedValues.Reset();
	OutEditedBounds = FVoxelIntBox();
	const bool bLogAbsorb = CVarDRSnowAbsorbLog.GetValueOnGameThread() != 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		OuterRadius <= 0.f || Strength <= 0.f || DistanceDivisor <= 0.f ||
		SweepRadius <= 0.f || MaxSweepsPerTick <= 0)
	{
		return 0.f;
	}

	const FVector Segment = TargetLocation - BrushOrigin;
	const float FrustumRange = Segment.Size();
	const FVector Direction = Segment.GetSafeNormal();
	if (FrustumRange <= KINDA_SMALL_NUMBER || Direction.IsNearlyZero())
	{
		return 0.f;
	}

	const float InnerRadius = OuterRadius * FMath::Clamp(InnerRadiusRatio, 0.f, 1.f);
	const float ClampedFarStrengthRatio = FMath::Clamp(FarStrengthRatio, 0.f, 1.f);
	// 표면 복셀 한 층이 충돌체와 겹치는 경우를 제거할 수 있도록, 최소 1cm와
	// 반 복셀 중 큰 값을 표면 허용 깊이로 사용한다.
	const float SurfaceAllowanceCm = FMath::Max(
		DRSnowAbsorbPlanes::SurfaceAbsorbAllowanceCm,
		VoxelWorld->VoxelSize * 0.5f);
	FVector OcclusionAxisY;
	FVector OcclusionAxisZ;
	Direction.FindBestAxisVectors(OcclusionAxisY, OcclusionAxisZ);
	DrawAbsorbRange(VoxelWorld->GetWorld(), BrushOrigin, TargetLocation, InnerRadius, OuterRadius);

	// 기본 slab 깊이가 너무 작아 설정된 횟수로 전체 range를 못 덮는 경우에는
	// slab을 자동으로 키운다. 따라서 고정된 작업 예산 안에서도 사각지대가 생기지 않는다.
	const float MinimumSliceDepth = FMath::Max(SweepRadius * 2.f, VoxelWorld->VoxelSize * 4.f);
	const float SliceDepth = FMath::Max(
		MinimumSliceDepth,
		FrustumRange / static_cast<float>(MaxSweepsPerTick));
	const int32 SliceCount = FMath::Clamp(
		FMath::CeilToInt(FrustumRange / SliceDepth),
		1,
		MaxSweepsPerTick);
	const FVector RadialExtentScale(
		FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(Direction.X))),
		FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(Direction.Y))),
		FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(Direction.Z))));
	TArray<FVoxelSurfaceEditsVoxel> AbsorbVoxels;
	TSet<FIntVector> CandidatePositions;
	FBox LocalCandidateBounds(ForceInit);
	int32 TotalSurfaceVoxelCount = 0;
	int32 OcclusionRejectedCount = 0;
	int32 RejectedSamples = 0;

	for (int32 SliceIndex = 0; SliceIndex < SliceCount; ++SliceIndex)
	{
		const float SliceStart = SliceIndex * SliceDepth;
		const float SliceEnd = FMath::Min(FrustumRange, SliceStart + SliceDepth);
		if (SliceEnd <= SliceStart)
		{
			break;
		}

		const float StartAlpha = SliceStart / FrustumRange;
		const float EndAlpha = SliceEnd / FrustumRange;
		const float SliceStartRadius = FMath::Lerp(InnerRadius, OuterRadius, StartAlpha);
		const float SliceEndRadius = FMath::Lerp(InnerRadius, OuterRadius, EndAlpha);
		const FVector SliceBeginLocation = BrushOrigin + Direction * SliceStart;
		const FVector SliceEndLocation = BrushOrigin + Direction * SliceEnd;

		// 원형 단면은 Direction에 수직이므로 진행축 방향으로 Radius만큼 뻗지 않는다.
		// 각 월드 축에 투영되는 원의 정확한 extent를 사용해야 인접 slab AABB가
		// 불필요하게 크게 겹치지 않는다.
		const FVector StartRadialExtent = RadialExtentScale * SliceStartRadius;
		const FVector EndRadialExtent = RadialExtentScale * SliceEndRadius;
		const FBox GlobalSliceBounds(
			(SliceBeginLocation - StartRadialExtent).ComponentMin(
				SliceEndLocation - EndRadialExtent),
			(SliceBeginLocation + StartRadialExtent).ComponentMax(
				SliceEndLocation + EndRadialExtent));

		FBox LocalSliceBounds(ForceInit);
		for (int32 X = 0; X < 2; ++X)
		{
			for (int32 Y = 0; Y < 2; ++Y)
			{
				for (int32 Z = 0; Z < 2; ++Z)
				{
					LocalSliceBounds += VoxelWorld->GlobalToLocalFloat(FVector(
						X == 0 ? GlobalSliceBounds.Min.X : GlobalSliceBounds.Max.X,
						Y == 0 ? GlobalSliceBounds.Min.Y : GlobalSliceBounds.Max.Y,
						Z == 0 ? GlobalSliceBounds.Min.Z : GlobalSliceBounds.Max.Z)).ToFloat();
				}
			}
		}

		const FVoxelIntBox SliceBounds(LocalSliceBounds);
		if (!SliceBounds.IsValid())
		{
			continue;
		}
		if (CVarDRSnowAbsorbDebugDraw.GetValueOnGameThread() != 0)
		{
			DrawDebugBox(
				VoxelWorld->GetWorld(),
				GlobalSliceBounds.GetCenter(),
				GlobalSliceBounds.GetExtent(),
				FColor::Blue,
				false,
				0.15f,
				0,
				0.75f);
		}

		FVoxelSurfaceEditsVoxels SurfaceVoxels;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Adaptive_QuerySurface);
			DRSnowSurfaceQuery::FindSurface(
				SurfaceVoxels,
				VoxelWorld,
				SliceBounds);
		}
		TotalSurfaceVoxelCount += SurfaceVoxels.Voxels->Num();

		for (const FVoxelSurfaceEditsVoxelBase& SourceVoxel : *SurfaceVoxels.Voxels)
		{
			const FVector VoxelLocation = VoxelWorld->LocalToGlobal(SourceVoxel.Position);
			const FVector Delta = VoxelLocation - BrushOrigin;
			const float DistanceAlong = FVector::DotProduct(Delta, Direction);
			if (DistanceAlong < SliceStart || DistanceAlong > SliceEnd)
			{
				continue;
			}

			const float AlongAlpha = DistanceAlong / FrustumRange;
			const float RadiusAtPoint = FMath::Lerp(InnerRadius, OuterRadius, AlongAlpha);
			const FVector RadialDelta = Delta - Direction * DistanceAlong;
			const float RadialAlpha = RadiusAtPoint > KINDA_SMALL_NUMBER
				? RadialDelta.Size() / RadiusAtPoint
				: (RadialDelta.IsNearlyZero() ? 0.f : BIG_NUMBER);
			if (RadialAlpha > 1.f)
			{
				continue;
			}
			if (IsBehindAbsorbOccluder(
				OcclusionDepths,
				OcclusionVolumes,
				SurfaceAllowanceCm,
				Delta,
				RadialDelta,
				OcclusionAxisY,
				OcclusionAxisZ,
				RadiusAtPoint,
				DistanceAlong,
				FrustumRange))
			{
				++OcclusionRejectedCount;
				if (CVarDRSnowAbsorbLog.GetValueOnGameThread() >= 2 && RejectedSamples++ < 4)
				{
					UE_LOG(LogDRSnowAbsorb, Log,
						TEXT("[RejectedVoxel] World=%s VoxelWorld=%s Local=%s Position=%s Origin=%s Distance=%.3f Radius=%.3f MaskCells=%d PlaneVolumes=%d"),
						*GetNameSafe(VoxelWorld->GetWorld()), *VoxelWorld->GetPathName(),
						*SourceVoxel.Position.ToString(), *VoxelLocation.ToCompactString(),
						*BrushOrigin.ToCompactString(), DistanceAlong, RadiusAtPoint, OcclusionDepths.Num(), OcclusionVolumes.Num());
				}
				continue;
			}

			const float DistanceWeight = FMath::Lerp(
				1.f,
				ClampedFarStrengthRatio,
				SmoothStep(AlongAlpha));
			const float RadialWeight = 1.f - SmoothStep(RadialAlpha);
			const float VoxelStrength = Strength * DistanceWeight * RadialWeight;
			if (VoxelStrength <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FIntVector CandidatePosition(
				SourceVoxel.Position.X,
				SourceVoxel.Position.Y,
				SourceVoxel.Position.Z);
			if (CandidatePositions.Contains(CandidatePosition))
			{
				continue;
			}
			CandidatePositions.Add(CandidatePosition);
			AbsorbVoxels.Add_GetRef(FVoxelSurfaceEditsVoxel(SourceVoxel)).Strength = VoxelStrength;
			LocalCandidateBounds += FVector(
				SourceVoxel.Position.X,
				SourceVoxel.Position.Y,
				SourceVoxel.Position.Z);
		}
	}

	if (AbsorbVoxels.IsEmpty() || !LocalCandidateBounds.IsValid)
	{
		if (bLogAbsorb)
		{
			UE_LOG(LogDRSnowAbsorb, Log,
				TEXT("Adaptive slab query found no editable surface: Slices=%d Depth=%.1f Range=%.1f Surface=%d OcclusionRejected=%d MaskCells=%d PlaneVolumes=%d"),
				SliceCount, SliceDepth, FrustumRange, TotalSurfaceVoxelCount,
				OcclusionRejectedCount, OcclusionDepths.Num(), OcclusionVolumes.Num());
		}
		return 0.f;
	}

	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Adaptive/SurfaceQueried"), TotalSurfaceVoxelCount);
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Adaptive/Candidates"), AbsorbVoxels.Num());
	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
	ProcessedVoxels.Bounds = FVoxelIntBox(LocalCandidateBounds.ExpandBy(1.f));
	ProcessedVoxels.Info.bHasValues = true;
	ProcessedVoxels.Info.bHasNormals = true;
	ProcessedVoxels.Info.bHasSurfacePositions = true;
	ProcessedVoxels.Info.bHasExactDistanceField = true;
	ProcessedVoxels.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(AbsorbVoxels));

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_EditValues);
		// 머티리얼 데이터 복구가 끝날 때까지 렌더 갱신을 보류한다. 렌더러가
		// 중간 밀도 전용 상태를 보지 않게 한다.
		UVoxelSurfaceEditTools::EditVoxelValues(
			OutModifiedValues,
			OutEditedBounds,
			VoxelWorld,
			ProcessedVoxels,
			DistanceDivisor,
			true,
			true,
			false);
	}
	RepairAbsorbMaterialsBeforeRender(
		VoxelWorld, ProcessedVoxels, OutModifiedValues, OutEditedBounds);
	if (bUpdateRender && OutEditedBounds.IsValid())
	{
		FVoxelToolHelpers::UpdateWorld(VoxelWorld, OutEditedBounds);
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Absorb/Adaptive/ModifiedValues"), OutModifiedValues.Num());

	float ModifiedAmount = 0.f;
	int32 Changed = 0, SolidToEmpty = 0, EmptyDensityOnly = 0, Samples = 0;
	for (const FModifiedVoxelValue& ModifiedValue : OutModifiedValues)
	{
		ModifiedAmount += GetRemovedSolidDensityAmount(ModifiedValue);
		if (ModifiedValue.NewValue == ModifiedValue.OldValue) { continue; }
		++Changed;
		SolidToEmpty += ModifiedValue.OldValue <= 0.f && ModifiedValue.NewValue > 0.f;
		EmptyDensityOnly += ModifiedValue.OldValue > 0.f && ModifiedValue.NewValue > 0.f;
		if (CVarDRSnowAbsorbLog.GetValueOnGameThread() >= 2 && Samples++ < 4)
		{
			UE_LOG(LogDRSnowAbsorb, Log,
				TEXT("[ChangedVoxel] VoxelWorld=%s Position=%s Old=%.6f New=%.6f"),
				*VoxelWorld->GetPathName(), *VoxelWorld->LocalToGlobal(ModifiedValue.Position).ToCompactString(),
				ModifiedValue.OldValue, ModifiedValue.NewValue);
		}
	}

	if (bLogAbsorb)
	{
		UE_LOG(LogDRSnowAbsorb, Log,
			TEXT("Adaptive slab edit: World=%s VoxelWorld=%s Origin=%s Slices=%d Depth=%.1f Range=%.1f Surface=%d Candidates=%d OcclusionRejected=%d MaskCells=%d PlaneVolumes=%d ModifiedRecords=%d Changed=%d SolidToEmpty=%d EmptyDensityOnly=%d Applied=%.6f"),
			*GetNameSafe(VoxelWorld->GetWorld()), *VoxelWorld->GetPathName(), *BrushOrigin.ToCompactString(),
			SliceCount, SliceDepth, FrustumRange, TotalSurfaceVoxelCount,
			ProcessedVoxels.Voxels->Num(), OcclusionRejectedCount, OcclusionDepths.Num(), OcclusionVolumes.Num(),
			OutModifiedValues.Num(), Changed, SolidToEmpty, EmptyDensityOnly, ModifiedAmount);
	}
	return ModifiedAmount;
}
