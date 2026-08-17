#include "DRVoxelTeamColorLibrary.h"

#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelPaintMaterial.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelWorld.h"

namespace
{
	FVoxelPaintMaterial MakeTeamPaintMaterial(
		EVoxelMaterialConfig MaterialConfig,
		int32 TeamId)
	{
		FVoxelPaintMaterial PaintMaterial;
		switch (MaterialConfig)
		{
		case EVoxelMaterialConfig::SingleIndex:
			PaintMaterial.Type = EVoxelPaintMaterialType::SingleIndex;
			PaintMaterial.SingleIndex.Channel.Channel = TeamId;
			break;

		case EVoxelMaterialConfig::MultiIndex:
			PaintMaterial.Type = EVoxelPaintMaterialType::MultiIndex;
			PaintMaterial.SingleIndex.Channel.Channel = TeamId;
			PaintMaterial.MultiIndex.TargetValue = 1.f;
			break;

		case EVoxelMaterialConfig::RGB:
		default:
			break;
		}

		return PaintMaterial;
	}
}

bool UDRVoxelTeamColorLibrary::PaintTeamSurfaceAtArea(
	AVoxelWorld* VoxelWorld,
	FVector WorldLocation,
	float Radius,
	int32 TeamId)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Radius <= 0.f)
	{
		return false;
	}

	const FVoxelIntBox SurfaceBounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			WorldLocation,
			Radius);
	if (!SurfaceBounds.IsValid())
	{
		return false;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(
		SurfaceVoxels,
		VoxelWorld,
		SurfaceBounds,
		true);

	FVoxelSurfaceEditsStack SurfaceStack;
	SurfaceStack.Add(
		UVoxelSurfaceTools::ApplyFalloff(
			VoxelWorld,
			EVoxelFalloff::Smooth,
			WorldLocation,
			Radius,
			0.35f));

	const FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels =
		UVoxelSurfaceTools::ApplyStack(SurfaceVoxels, SurfaceStack);

	return PaintProcessedTeamSurface(
		VoxelWorld,
		ProcessedVoxels,
		TeamId,
		true);
}

bool UDRVoxelTeamColorLibrary::PaintProcessedTeamSurface(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	int32 TeamId,
	bool bUpdateRender)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	if (VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Voxel][TeamColor] Paint skipped. VoxelWorld=%s MaterialConfig=RGB. Use SingleIndex or MultiIndex for team material tests."),
			*GetNameSafe(VoxelWorld));
		return false;
	}

	TArray<FModifiedVoxelMaterial> ModifiedMaterials;
	FVoxelIntBox EditedMaterialBounds;
	UVoxelSurfaceEditTools::EditVoxelMaterials(
		ModifiedMaterials,
		EditedMaterialBounds,
		VoxelWorld,
		MakeTeamPaintMaterial(
			VoxelWorld->MaterialConfig,
			TeamId),
		ProcessedVoxels,
		true,
		false,
		bUpdateRender);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Voxel][TeamColor] Painted VoxelWorld=%s Config=%d TeamId=%d MaterialIndex=%d Materials=%d BoundsValid=%d"),
		*GetNameSafe(VoxelWorld),
		static_cast<int32>(VoxelWorld->MaterialConfig),
		TeamId,
		TeamId,
		ModifiedMaterials.Num(),
		EditedMaterialBounds.IsValid());

	return EditedMaterialBounds.IsValid();
}
