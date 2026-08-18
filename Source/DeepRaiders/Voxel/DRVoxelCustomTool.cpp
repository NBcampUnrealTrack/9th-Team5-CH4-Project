#include "DRVoxelCustomTool.h"

#include "VoxelData/VoxelDataImpl.inl"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelWorld.h"

namespace
{
float GetModifiedValueAmount(const TArray<FModifiedVoxelValue>& ModifiedValues)
{
	float ModifiedValueAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		ModifiedValueAmount += FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue);
	}

	return ModifiedValueAmount;
}

float GetSurfaceToolTargetValue(const FVoxelSurfaceEditsVoxel& SurfaceVoxel, float DistanceDivisor)
{
	// FVoxelSurfaceEditToolsImpl::EditVoxelValues와 같은 target 값이다.
	// 차이는 아래 write 단계에서 반대 방향 변화는 버린다는 점이다.
	return (SurfaceVoxel.Value + SurfaceVoxel.Strength) / DistanceDivisor;
}
}

UDRVoxelCustomTool::UDRVoxelCustomTool()
{
	ToolName = TEXT("DR Voxel Custom Tool");
}

void UDRVoxelCustomTool::GetToolConfig(FVoxelToolBaseConfig& OutConfig) const
{
	OutConfig.bHasAlignment = true;
	OutConfig.Alignment = EVoxelToolAlignment::Surface;
}

FVoxelIntBoxWithValidity UDRVoxelCustomTool::DoEdit()
{
	AVoxelWorld* World = GetVoxelWorld();
	if (!IsValid(World) || !World->IsCreated() || !SharedConfig)
	{
		return {};
	}

	const float Radius = SharedConfig->BrushSize / 2.f;
	const FVoxelSurfaceEditsProcessedVoxels SurfaceFootprint =
		FindSurfaceFootprint(
			World,
			GetToolPosition(),
			Radius,
			Falloff,
			Strength,
			GetTickData().IsAlternativeMode() ? !bAdd : bAdd);

	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelIntBox EditedBounds;
	ApplySurfaceVolumeEdit(
		World,
		SurfaceFootprint,
		DistanceDivisor,
		GetTickData().IsAlternativeMode() ? !bAdd : bAdd,
		ModifiedValues,
		EditedBounds,
		false);

	return EditedBounds.IsValid() ? FVoxelIntBoxWithValidity(EditedBounds) : FVoxelIntBoxWithValidity();
}

FVoxelSurfaceEditsProcessedVoxels UDRVoxelCustomTool::FindSurfaceFootprint(
	AVoxelWorld* VoxelWorld,
	const FVector& WorldLocation,
	float Radius,
	float Falloff,
	float Strength,
	bool bAdd)
{
	FVoxelSurfaceEditsProcessedVoxels EmptyResult;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Radius <= 0.f || Strength <= 0.f)
	{
		return EmptyResult;
	}

	const FVoxelIntBox SurfaceBounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			WorldLocation,
			Radius);
	if (!SurfaceBounds.IsValid())
	{
		return EmptyResult;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(
		SurfaceVoxels,
		VoxelWorld,
		SurfaceBounds,
		true);

	// SurfaceTool과 같은 브러시 결과를 만든다.
	// 여기서 나온 Strength의 부호가 Add/Remove 방향 판정에 사용된다.
	FVoxelSurfaceEditsStack SurfaceStack;
	SurfaceStack.Add(
		UVoxelSurfaceTools::ApplyFalloff(
			VoxelWorld,
			EVoxelFalloff::Smooth,
			WorldLocation,
			Radius,
			Falloff));
	SurfaceStack.Add(
		UVoxelSurfaceTools::ApplyConstantStrength(
			Strength * (bAdd ? -1.f : 1.f)));

	return UVoxelSurfaceTools::ApplyStack(SurfaceVoxels, SurfaceStack);
}

float UDRVoxelCustomTool::ApplySurfaceVolumeEdit(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
	float DistanceDivisor,
	bool bAdd,
	TArray<FModifiedVoxelValue>& ModifiedValues,
	FVoxelIntBox& EditedBounds,
	bool bUpdateRender)
{
	ModifiedValues.Reset();
	EditedBounds = FVoxelIntBox();

	if (!IsValid(VoxelWorld) ||
		!VoxelWorld->IsCreated() ||
		DistanceDivisor <= 0.f ||
		SurfaceFootprint.Voxels->Num() == 0)
	{
		return 0.f;
	}

	const FVoxelIntBox Bounds = SurfaceFootprint.Bounds;
	if (!Bounds.IsValid())
	{
		return 0.f;
	}
	EditedBounds = Bounds;

	TMap<FIntVector, float> TargetValueByPosition;
	for (const FVoxelSurfaceEditsVoxel& SurfaceVoxel : *SurfaceFootprint.Voxels)
	{
		const float SignedStrength = SurfaceVoxel.Strength;
		// SurfaceTool은 target 값을 그대로 쓰지만, 이 툴은 요청 방향과 반대인 후보를 버린다.
		if ((bAdd && SignedStrength >= 0.f) || (!bAdd && SignedStrength <= 0.f))
		{
			continue;
		}

		const float TargetValue = GetSurfaceToolTargetValue(SurfaceVoxel, DistanceDivisor);
		float& StoredValue = TargetValueByPosition.FindOrAdd(SurfaceVoxel.Position, TargetValue);
		StoredValue = bAdd ? FMath::Min(StoredValue, TargetValue) : FMath::Max(StoredValue, TargetValue);
	}

	if (TargetValueByPosition.Num() == 0)
	{
		EditedBounds = FVoxelIntBox();
		return 0.f;
	}

	FVoxelData& Data = VoxelWorld->GetData();
	TVoxelDataImpl<FModifiedVoxelValue> DataImpl(Data, false, true);
	{
		FVoxelWriteScopeLock Lock(Data, Bounds, FUNCTION_FNAME);
		DataImpl.Set<FVoxelValue>(Bounds, [&](int32 X, int32 Y, int32 Z, FVoxelValue& Value)
		{
			const float* TargetValue = TargetValueByPosition.Find(FIntVector(X, Y, Z));
			if (!TargetValue)
			{
				return;
			}

			const float CurrentValue = Value.ToFloat();
			// FVoxelValue는 값이 낮을수록 filled, 높을수록 empty 쪽이다.
			// 그래서 Add는 더 낮은 값만, Remove는 더 높은 값만 반영한다.
			if ((bAdd && *TargetValue < CurrentValue) || (!bAdd && *TargetValue > CurrentValue))
			{
				Value = FVoxelValue(*TargetValue);
			}
		});
	}

	ModifiedValues = MoveTemp(DataImpl.ModifiedValues);
	if (bUpdateRender && EditedBounds.IsValid())
	{
		FVoxelToolHelpers::UpdateWorld(VoxelWorld, EditedBounds);
	}

	return GetModifiedValueAmount(ModifiedValues);
}

FVoxelSurfaceEditsProcessedVoxels UDRVoxelCustomTool::MakeModifiedValueVoxelGroup(
	const FVoxelIntBox& Bounds,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	bool bAdd)
{
	TArray<FVoxelSurfaceEditsVoxel> Voxels;
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		const bool bMovedToFilled = ModifiedValue.OldValue > 0.f && ModifiedValue.NewValue < ModifiedValue.OldValue;
		const bool bMovedToEmpty = ModifiedValue.OldValue < ModifiedValue.NewValue;
		if ((bAdd && !bMovedToFilled) || (!bAdd && !bMovedToEmpty))
		{
			continue;
		}

		FVoxelSurfaceEditsVoxel Voxel;
		Voxel.Position = ModifiedValue.Position;
		Voxel.Strength = 1.f;
		Voxels.Add(Voxel);
	}

	FVoxelSurfaceEditsProcessedVoxels Result;
	Result.Bounds = Bounds;
	Result.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(Voxels));
	return Result;
}
