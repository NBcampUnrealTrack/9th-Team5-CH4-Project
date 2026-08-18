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
		const int32 MaterialIndex =
			UDRVoxelTeamColorLibrary::GetTeamMaterialIndex(TeamId);

		FVoxelPaintMaterial PaintMaterial;
		// 팀 재질 테스트는 SingleIndex/MultiIndex에서 material index를 바꾸는 방식으로 처리한다.
		switch (MaterialConfig)
		{
		case EVoxelMaterialConfig::SingleIndex:
			PaintMaterial.Type = EVoxelPaintMaterialType::SingleIndex;
			PaintMaterial.SingleIndex.Channel.Channel = MaterialIndex;
			break;

		case EVoxelMaterialConfig::MultiIndex:
			PaintMaterial.Type = EVoxelPaintMaterialType::MultiIndex;
			PaintMaterial.SingleIndex.Channel.Channel = MaterialIndex;
			PaintMaterial.MultiIndex.TargetValue = 1.f;
			break;

		case EVoxelMaterialConfig::RGB:
		default:
			break;
		}

		return PaintMaterial;
	}
}

int32 UDRVoxelTeamColorLibrary::GetTeamMaterialIndex(int32 TeamId)
{
	return TeamId == INDEX_NONE
		? 0
		: FMath::Max(0, TeamId) + 1;
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

	// Paint만 호출하는 경우에도 값 편집과 비슷한 표면 범위를 잡기 위해 falloff stack을 적용한다.
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

	if (ProcessedVoxels.Voxels->Num() == 0)
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
		GetTeamMaterialIndex(TeamId),
		ModifiedMaterials.Num(),
		EditedMaterialBounds.IsValid());

	return EditedMaterialBounds.IsValid();
}
