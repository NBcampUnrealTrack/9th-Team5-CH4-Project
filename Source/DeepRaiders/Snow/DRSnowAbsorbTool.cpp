#include "DRSnowAbsorbTool.h"

#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelWorld.h"

namespace
{
TAutoConsoleVariable<int32> CVarDRSnowAbsorbDebugDraw(
	TEXT("dr.Snow.Absorb.DebugDraw"),
	1,
	TEXT("Draw the active snow absorb range. 0: Off, 1: On"),
	ECVF_Default);

float SmoothStep(const float Value)
{
	const float ClampedValue = FMath::Clamp(Value, 0.f, 1.f);
	return ClampedValue * ClampedValue * (3.f - 2.f * ClampedValue);
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
	OutModifiedValues.Reset();
	OutEditedBounds = FVoxelIntBox();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || OuterRadius <= 0.f ||
		Strength <= 0.f || DistanceDivisor <= 0.f)
	{
		return 0.f;
	}

	const FVector Segment = TargetLocation - BrushOrigin;
	const float Length = Segment.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
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
		return 0.f;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(SurfaceVoxels, VoxelWorld, Bounds, true);

	const float InnerRadius = OuterRadius * FMath::Clamp(InnerRadiusRatio, 0.f, 1.f);
	const float ClampedFarStrengthRatio = FMath::Clamp(FarStrengthRatio, 0.f, 1.f);
	TArray<FVoxelSurfaceEditsVoxel> AbsorbVoxels;
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

		// 시작점(흡수구)으로 갈수록 강해지고, 외곽은 자연스럽게 약해진다.
		const float DistanceWeight = FMath::Lerp(1.f, ClampedFarStrengthRatio, SmoothStep(AlongAlpha));
		const float RadialWeight = 1.f - SmoothStep(RadialAlpha);
		const float VoxelStrength = Strength * DistanceWeight * RadialWeight;
		if (VoxelStrength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FVoxelSurfaceEditsVoxel& AbsorbVoxel = AbsorbVoxels.Add_GetRef(FVoxelSurfaceEditsVoxel(SourceVoxel));
		AbsorbVoxel.Strength = VoxelStrength;
	}

	if (AbsorbVoxels.IsEmpty())
	{
		return 0.f;
	}

	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
	ProcessedVoxels.Bounds = Bounds;
	ProcessedVoxels.Info.bHasValues = true;
	ProcessedVoxels.Info.bHasNormals = true;
	ProcessedVoxels.Info.bHasSurfacePositions = true;
	ProcessedVoxels.Info.bHasExactDistanceField = true;
	ProcessedVoxels.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(AbsorbVoxels));

	UVoxelSurfaceEditTools::EditVoxelValues(
		OutModifiedValues,
		OutEditedBounds,
		VoxelWorld,
		ProcessedVoxels,
		DistanceDivisor,
		true,
		true,
		bUpdateRender);

	float ModifiedAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : OutModifiedValues)
	{
		ModifiedAmount += FMath::Max(0.f, ModifiedValue.NewValue - ModifiedValue.OldValue);
	}
	return ModifiedAmount;
}

float UDRSnowAbsorbTool::RemoveSnowAtSample(
	AVoxelWorld* VoxelWorld,
	const FVector& SampleLocation,
	const float Radius,
	TArray<FModifiedVoxelValue>& OutModifiedValues,
	FVoxelIntBox& OutEditedBounds,
	const bool bUpdateRender)
{
	OutModifiedValues.Reset();
	OutEditedBounds = FVoxelIntBox();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Radius <= 0.f)
	{
		return 0.f;
	}

	if (CVarDRSnowAbsorbDebugDraw.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(
			VoxelWorld->GetWorld(),
			SampleLocation,
			Radius,
			16,
			FColor::Orange,
			false,
			0.15f,
			0,
			1.5f);
	}

	UVoxelSphereTools::RemoveSphere(
		OutModifiedValues,
		OutEditedBounds,
		VoxelWorld,
		SampleLocation,
		Radius,
		false,
		true,
		true,
		bUpdateRender);

	float ModifiedAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : OutModifiedValues)
	{
		ModifiedAmount += FMath::Max(0.f, ModifiedValue.NewValue - ModifiedValue.OldValue);
	}
	return ModifiedAmount;
}

float UDRSnowAbsorbTool::RemoveSnowFromFrustumSlice(
	AVoxelWorld* VoxelWorld,
	const FVector& FrustumOrigin,
	const FVector& FrustumDirection,
	const float FrustumRange,
	const float OuterRadius,
	const float SliceStart,
	const float SliceLength,
	const float Strength,
	const float DistanceDivisor,
	TArray<FModifiedVoxelValue>& OutModifiedValues,
	FVoxelIntBox& OutEditedBounds,
	const bool bUpdateRender)
{
	OutModifiedValues.Reset();
	OutEditedBounds = FVoxelIntBox();
	const FVector Direction = FrustumDirection.GetSafeNormal();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Direction.IsNearlyZero() ||
		FrustumRange <= 0.f || OuterRadius <= 0.f || SliceLength <= 0.f || Strength <= 0.f)
	{
		return 0.f;
	}

	const float ClampedSliceStart = FMath::Clamp(SliceStart, 0.f, FrustumRange);
	const float ClampedSliceEnd = FMath::Clamp(SliceStart + SliceLength, 0.f, FrustumRange);
	if (ClampedSliceEnd <= ClampedSliceStart)
	{
		return 0.f;
	}

	const FVector SliceBegin = FrustumOrigin + Direction * ClampedSliceStart;
	const FVector SliceEnd = FrustumOrigin + Direction * ClampedSliceEnd;
	const FBox GlobalBounds(
		SliceBegin.ComponentMin(SliceEnd) - FVector(OuterRadius),
		SliceBegin.ComponentMax(SliceEnd) + FVector(OuterRadius));
	FBox LocalBounds(ForceInit);
	for (int32 X = 0; X < 2; ++X)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 Z = 0; Z < 2; ++Z)
			{
				LocalBounds += VoxelWorld->GlobalToLocalFloat(FVector(
					X == 0 ? GlobalBounds.Min.X : GlobalBounds.Max.X,
					Y == 0 ? GlobalBounds.Min.Y : GlobalBounds.Max.Y,
					Z == 0 ? GlobalBounds.Min.Z : GlobalBounds.Max.Z)).ToFloat();
			}
		}
	}
	const FVoxelIntBox Bounds(LocalBounds);
	if (!Bounds.IsValid())
	{
		return 0.f;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(SurfaceVoxels, VoxelWorld, Bounds, true);
	TArray<FVoxelSurfaceEditsVoxel> SliceVoxels;
	for (const FVoxelSurfaceEditsVoxelBase& SourceVoxel : *SurfaceVoxels.Voxels)
	{
		const FVector Delta = VoxelWorld->LocalToGlobal(SourceVoxel.Position) - FrustumOrigin;
		const float DistanceAlong = FVector::DotProduct(Delta, Direction);
		if (DistanceAlong < ClampedSliceStart || DistanceAlong > ClampedSliceEnd)
		{
			continue;
		}

		const float RangeAlpha = DistanceAlong / FrustumRange;
		const float RadiusAtPoint = FMath::Lerp(OuterRadius * 0.2f, OuterRadius, RangeAlpha);
		const float RadialAlpha = (Delta - Direction * DistanceAlong).Size() / RadiusAtPoint;
		if (RadialAlpha > 1.f)
		{
			continue;
		}

		FVoxelSurfaceEditsVoxel& SliceVoxel = SliceVoxels.Add_GetRef(FVoxelSurfaceEditsVoxel(SourceVoxel));
		SliceVoxel.Strength = Strength * (1.f - SmoothStep(RadialAlpha));
	}
	if (SliceVoxels.IsEmpty())
	{
		return 0.f;
	}

	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
	ProcessedVoxels.Bounds = Bounds;
	ProcessedVoxels.Info = SurfaceVoxels.Info;
	ProcessedVoxels.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(SliceVoxels));
	UVoxelSurfaceEditTools::EditVoxelValues(
		OutModifiedValues, OutEditedBounds, VoxelWorld, ProcessedVoxels, DistanceDivisor, true, true, bUpdateRender);

	float ModifiedAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : OutModifiedValues)
	{
		ModifiedAmount += FMath::Max(0.f, ModifiedValue.NewValue - ModifiedValue.OldValue);
	}
	return ModifiedAmount;
}
