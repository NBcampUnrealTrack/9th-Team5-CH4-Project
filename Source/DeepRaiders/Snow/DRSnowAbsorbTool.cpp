#include "DRSnowAbsorbTool.h"
#include "DRSnowSurfaceQuery.h"

#include "DrawDebugHelpers.h"
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
	TEXT("Draw the active snow absorb range. 0: Off, 1: On"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarDRSnowAbsorbLog(
	TEXT("dr.Snow.Absorb.Log"),
	0,
	TEXT("Log snow absorb pipeline. 0: Off, 1: On"),
	ECVF_Default);

float SmoothStep(const float Value)
{
	const float ClampedValue = FMath::Clamp(Value, 0.f, 1.f);
	return ClampedValue * ClampedValue * (3.f - 2.f * ClampedValue);
}

// Repair material data after the density edit but before the first render update.
// Unlike FVoxelSurfaceEditToolsImpl::PropagateVoxelMaterials this path never
// asserts when a surface sample has no filled neighbor: it simply leaves that
// sample unchanged. Only voxels that actually changed density are considered.
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
		// Hold the render update until material data is repaired. This prevents the
		// renderer from ever seeing the intermediate density-only state.
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
		ModifiedAmount += FMath::Max(0.f, ModifiedValue.NewValue - ModifiedValue.OldValue);
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
				TEXT("Adaptive slab query found no editable surface: Slices=%d Depth=%.1f Range=%.1f Surface=%d"),
				SliceCount, SliceDepth, FrustumRange, TotalSurfaceVoxelCount);
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
		// Hold the render update until material data is repaired. This prevents the
		// renderer from ever seeing the intermediate density-only state.
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
	for (const FModifiedVoxelValue& ModifiedValue : OutModifiedValues)
	{
		ModifiedAmount += FMath::Max(0.f, ModifiedValue.NewValue - ModifiedValue.OldValue);
	}

	if (bLogAbsorb)
	{
		UE_LOG(LogDRSnowAbsorb, Log,
			TEXT("Adaptive slab edit: Slices=%d Depth=%.1f Range=%.1f Surface=%d Candidates=%d Modified=%d Applied=%.4f"),
			SliceCount, SliceDepth, FrustumRange, TotalSurfaceVoxelCount,
			ProcessedVoxels.Voxels->Num(), OutModifiedValues.Num(), ModifiedAmount);
	}
	return ModifiedAmount;
}
