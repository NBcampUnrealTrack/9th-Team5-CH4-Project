#include "DRSnowSurfaceSubsystem.h"

#include "EngineUtils.h"
#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelWorld.h"

bool UDRSnowSurfaceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

float UDRSnowSurfaceSubsystem::RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request)
{
	if (Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return 0.f;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return 0.f;
	}

	constexpr float DistanceDivisor = 4.f;
	constexpr float SurfaceFalloff = 0.35f;

	// SurfaceTool 객체를 직접 쓰지 않고 함수형 API만 감싼다.
	// 이 Subsystem은 Voxel Plugin 세부 호출을 숨기는 wrapper 역할을 한다.
	const FVoxelIntBox SurfaceBounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius);
	
	if (!SurfaceBounds.IsValid())
	{
		return 0.f;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(
		SurfaceVoxels,
		VoxelWorld,
		SurfaceBounds,
		true);

	// 흡수는 표면을 따라 밀도를 조정해야 하므로 RemoveSphere가 아니라
	// surface voxel 탐색 + falloff + constant strength 조합으로 처리한다.
	FVoxelSurfaceEditsStack SurfaceStack;
	SurfaceStack.Add(
		UVoxelSurfaceTools::ApplyFalloff(
			VoxelWorld,
			EVoxelFalloff::Smooth,
			Request.WorldLocation,
			Request.Radius,
			SurfaceFalloff));
	SurfaceStack.Add(
		UVoxelSurfaceTools::ApplyConstantStrength(
			Request.RequestedAmount *
			(Request.bInvertSurfaceStrength ? -1.f : 1.f)));

	const FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels = UVoxelSurfaceTools::ApplyStack(SurfaceVoxels, SurfaceStack);

	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelIntBox EditedBounds;
	UVoxelSurfaceEditTools::EditVoxelValues(
		ModifiedValues,
		EditedBounds,
		VoxelWorld,
		ProcessedVoxels,
		DistanceDivisor,
		true,
		true,
		true);

	float ModifiedValueAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		ModifiedValueAmount += FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue);
	}

	// 실제 Voxel 값 변화량만 흡수량으로 인정한다.
	// 이 값이 이후 SnowAmmo 회복과 SnowLedger 감소량의 기준이 된다.
	const float RemovedAmount = FMath::Min(Request.RequestedAmount, ModifiedValueAmount);
	if (RemovedAmount > 0.f)
	{
		OnSnowRemovedFromSurface.Broadcast(Request, RemovedAmount);
	}

	return RemovedAmount;
}

AVoxelWorld* UDRSnowSurfaceSubsystem::ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const
{
	if (IsValid(Request.TargetVoxelWorld.Get()))
	{
		return Request.TargetVoxelWorld.Get();
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}
