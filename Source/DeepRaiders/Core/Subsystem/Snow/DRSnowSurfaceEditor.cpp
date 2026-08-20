#include "DRSnowSurfaceEditor.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Voxel/DRDirectionalSurfaceTool.h"
#include "DeepRaiders/Voxel/DRVoxelTeamColorLibrary.h"
#include "EngineUtils.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelWorld.h"

namespace
{
constexpr float SnowSurfaceDistanceDivisor = 4.f;
constexpr float SnowSurfaceFalloff = 0.35f;

float GetModifiedValueAmount(const TArray<FModifiedVoxelValue>& ModifiedValues)
{
	float ModifiedValueAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		ModifiedValueAmount += FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue);
	}

	return ModifiedValueAmount;
}

FVoxelSurfaceEditsProcessedVoxels MakeProcessedVoxelGroup(
	const FVoxelSurfaceEditsProcessedVoxels& SourceVoxels,
	TArray<FVoxelSurfaceEditsVoxel>&& GroupVoxels)
{
	FVoxelSurfaceEditsProcessedVoxels Result;
	Result.Bounds = SourceVoxels.Bounds;
	Result.Info = SourceVoxels.Info;
	Result.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(GroupVoxels));
	return Result;
}

FVoxelSurfaceEditsProcessedVoxels MakeNewlyAddedVoxelGroup(
	const FVoxelSurfaceEditsProcessedVoxels& SourceVoxels,
	const TArray<FModifiedVoxelValue>& ModifiedValues)
{
	TSet<FIntVector> NewlyAddedPositions;
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		// FVoxelValue에서 양수는 empty, 음수/0은 filled 쪽이다.
		// 기존에 이미 차 있던 표면은 다른 팀 재질일 수 있으니 Add paint 대상에서 제외한다.
		if (ModifiedValue.OldValue > 0.f &&
			ModifiedValue.NewValue < ModifiedValue.OldValue)
		{
			NewlyAddedPositions.Add(ModifiedValue.Position);
		}
	}

	TArray<FVoxelSurfaceEditsVoxel> NewVoxels;
	for (const FVoxelSurfaceEditsVoxel& Voxel : *SourceVoxels.Voxels)
	{
		if (NewlyAddedPositions.Contains(Voxel.Position))
		{
			NewVoxels.Add(Voxel);
		}
	}

	return MakeProcessedVoxelGroup(SourceVoxels, MoveTemp(NewVoxels));
}


}

FDRSnowSurfaceEditResult FDRSnowSurfaceEditor::AddSnowAtArea(
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowSurfaceEditResult Result;
	if (Request.Radius <= 0.f || Request.Amount <= 0.f)
	{
		return Result;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		// Directional 도구의 실제 변경 목록은 Subsystem이 Ownership/Volume 원본 데이터를 갱신할 때 사용한다.
		const FVoxelSurfaceEditsProcessedVoxels SurfaceFootprint =
			UDRDirectionalSurfaceTool::FindSurfaceFootprint(
				VoxelWorld,
				Request.WorldLocation,
				Request.Radius,
				SnowSurfaceFalloff,
				Request.Amount,
				true);

		TArray<FModifiedVoxelValue> ModifiedValues;
		FVoxelIntBox EditedBounds;
		const float ModifiedValueAmount = UDRDirectionalSurfaceTool::ApplySurfaceVolumeEdit(
			VoxelWorld,
			SurfaceFootprint,
			SnowSurfaceDistanceDivisor,
			true,
			ModifiedValues,
			EditedBounds);
		Result.AppliedAmount = FMath::Min(Request.Amount, ModifiedValueAmount);
		if (Result.AppliedAmount > 0.f)
		{
			if (EditedBounds.IsValid())
			{
				UDRVoxelTeamColorLibrary::PaintProcessedTeamSurface(
					VoxelWorld,
					UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
						EditedBounds,
						ModifiedValues,
						true),
					Request.Context.TeamId,
					true);
			}

			Result.VoxelWorld = VoxelWorld;
			Result.ModifiedValues = MoveTemp(ModifiedValues);
			Result.bUseModifiedValuesForVolume = true;
		}

		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::SphereTool)
	{
		// SphereTool은 표면을 찾지 않고 구 부피 안의 Voxel 값을 직접 추가한다.
		TArray<FModifiedVoxelValue> ModifiedValues;
		FVoxelIntBox EditedBounds;
		UVoxelSphereTools::AddSphere(
			ModifiedValues,
			EditedBounds,
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius,
			true,
			true,
			true,
			true);

		const float ModifiedValueAmount = GetModifiedValueAmount(ModifiedValues);

		Result.AppliedAmount = FMath::Min(Request.Amount, ModifiedValueAmount);
		if (Result.AppliedAmount > 0.f)
		{
			const FVoxelIntBox SurfaceBounds =
				UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
					VoxelWorld,
					Request.WorldLocation,
					Request.Radius);
			if (SurfaceBounds.IsValid())
			{
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
						Request.WorldLocation,
						Request.Radius,
						SnowSurfaceFalloff));

				const FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels =
					UVoxelSurfaceTools::ApplyStack(SurfaceVoxels, SurfaceStack);
				UDRVoxelTeamColorLibrary::PaintProcessedTeamSurface(
					VoxelWorld,
					MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
					Request.Context.TeamId,
					true);
			}

		}

		Result.VoxelWorld = VoxelWorld;
		return Result;
	}

	// 눈 쌓기도 SurfaceTool 객체 대신 함수형 API로 처리한다.
	// 이 경계를 유지해야 투사체/장판/맵 장치가 같은 Voxel 표현 함수를 공유할 수 있다.
	const FVoxelIntBox SurfaceBounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius);
	if (!SurfaceBounds.IsValid())
	{
		return Result;
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
			Request.WorldLocation,
			Request.Radius,
			SnowSurfaceFalloff));
	SurfaceStack.Add(
		UVoxelSurfaceTools::ApplyConstantStrength(-Request.Amount));

	const FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels =
		UVoxelSurfaceTools::ApplyStack(SurfaceVoxels, SurfaceStack);

	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelIntBox EditedBounds;
	UVoxelSurfaceEditTools::EditVoxelValues(
		ModifiedValues,
		EditedBounds,
		VoxelWorld,
		ProcessedVoxels,
		SnowSurfaceDistanceDivisor,
		true,
		true,
		true);

	const float ModifiedValueAmount = GetModifiedValueAmount(ModifiedValues);

	Result.AppliedAmount = FMath::Min(Request.Amount, ModifiedValueAmount);
	if (Result.AppliedAmount > 0.f)
	{
		// 팀 소유 표현은 FVoxelValue에 섞지 않고 material index paint로만 처리한다.
		// 단, 기존 표면과 겹친 교집합은 유지하고 이번 Add로 새로 채워진 위치만 칠한다.
		UDRVoxelTeamColorLibrary::PaintProcessedTeamSurface(
			VoxelWorld,
			MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
			Request.Context.TeamId,
			true);

	}

	Result.VoxelWorld = VoxelWorld;
	return Result;
}

FDRSnowSurfaceEditResult FDRSnowSurfaceEditor::RemoveSnowAtArea(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	FDRSnowSurfaceEditResult Result;
	if (Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return Result;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		const FVoxelSurfaceEditsProcessedVoxels SurfaceFootprint =
			UDRDirectionalSurfaceTool::FindSurfaceFootprint(
				VoxelWorld,
				Request.WorldLocation,
				Request.Radius,
				SnowSurfaceFalloff,
				Request.RequestedAmount,
				Request.bInvertSurfaceStrength);

		TArray<FModifiedVoxelValue> ModifiedValues;
		FVoxelIntBox EditedBounds;
		const float ModifiedValueAmount = UDRDirectionalSurfaceTool::ApplySurfaceVolumeEdit(
			VoxelWorld,
			SurfaceFootprint,
			SnowSurfaceDistanceDivisor,
			Request.bInvertSurfaceStrength,
			ModifiedValues,
			EditedBounds);
		Result.AppliedAmount = FMath::Min(Request.RequestedAmount, ModifiedValueAmount);
		if (Result.AppliedAmount > 0.f)
		{
			Result.VoxelWorld = VoxelWorld;
			Result.ModifiedValues = MoveTemp(ModifiedValues);
			Result.bUseModifiedValuesForVolume = true;
		}

		return Result;
	}

	if (Request.EditTool == EDRSnowVoxelEditTool::SphereTool)
	{
		TArray<FModifiedVoxelValue> ModifiedValues;
		FVoxelIntBox EditedBounds;
		UVoxelSphereTools::RemoveSphere(
			ModifiedValues,
			EditedBounds,
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius,
			true,
			true,
			true,
			true);

		UDRVoxelTeamColorLibrary::PaintTeamSurfaceAtArea(
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius,
			INDEX_NONE);

		const float ModifiedValueAmount = GetModifiedValueAmount(ModifiedValues);

		Result.AppliedAmount = FMath::Min(Request.RequestedAmount, ModifiedValueAmount);
		Result.VoxelWorld = VoxelWorld;
		return Result;
	}

	// SurfaceTool 객체를 직접 쓰지 않고 함수형 API만 감싼다.
	const FVoxelIntBox SurfaceBounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius);
	
	if (!SurfaceBounds.IsValid())
	{
		return Result;
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
			SnowSurfaceFalloff));
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
		SnowSurfaceDistanceDivisor,
		true,
		true,
		true);

	const float ModifiedValueAmount = GetModifiedValueAmount(ModifiedValues);

	// 실제 Voxel 값 변화량만 흡수량으로 인정한다.
	// 이 값이 이후 SnowAmmo 회복과 SnowLedger 감소량의 기준이 된다.
	Result.AppliedAmount = FMath::Min(Request.RequestedAmount, ModifiedValueAmount);
	Result.VoxelWorld = VoxelWorld;
	return Result;
}

bool FDRSnowSurfaceEditor::RepaintSnowMaterialsAtArea(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore)
{
	if (Request.Radius <= 0.f)
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	const FVoxelIntBox SurfaceBounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius);
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
			Request.WorldLocation,
			Request.Radius,
			SnowSurfaceFalloff));

	const FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels =
		UVoxelSurfaceTools::ApplyStack(SurfaceVoxels, SurfaceStack);

	TMap<int32, TArray<FVoxelSurfaceEditsVoxel>> VoxelsByTeam;
	for (const FVoxelSurfaceEditsVoxel& Voxel : *ProcessedVoxels.Voxels)
	{
		int32 DominantTeamId = INDEX_NONE;
		// 새로 생긴 눈은 ownership 기록이 더 정확하고, 기존 표면은 Volume 우세 팀으로 fallback 한다.
		const bool bFoundOwnership = OwnershipStore.GetNearestTeamAtVoxel(
				VoxelWorld,
				Voxel.Position,
				2,
				DominantTeamId);

		if (!bFoundOwnership)
		{
			const FVector SampleWorldLocation =
				ProcessedVoxels.Info.bHasSurfacePositions
					? VoxelWorld->LocalToGlobalFloat(FVoxelVector(Voxel.SurfacePosition))
					: VoxelWorld->LocalToGlobal(Voxel.Position);
			DominantTeamId = VolumeStore.GetDominantTeamAtLocation(SampleWorldLocation);
		}

		VoxelsByTeam.FindOrAdd(DominantTeamId).Add(Voxel);
	}

	bool bPaintedAny = false;
	for (TPair<int32, TArray<FVoxelSurfaceEditsVoxel>>& TeamVoxels : VoxelsByTeam)
	{
		if (TeamVoxels.Value.Num() == 0)
		{
			continue;
		}

		bPaintedAny |= UDRVoxelTeamColorLibrary::PaintProcessedTeamSurface(
			VoxelWorld,
			MakeProcessedVoxelGroup(ProcessedVoxels, MoveTemp(TeamVoxels.Value)),
			TeamVoxels.Key,
			true);
	}

	return bPaintedAny;
}

AVoxelWorld* FDRSnowSurfaceEditor::ResolveVoxelWorld(const FDRSnowSurfaceAddRequest& Request) const
{
	if (IsValid(Request.TargetVoxelWorld.Get()))
	{
		return Request.TargetVoxelWorld.Get();
	}

	UWorld* LocalWorld = this->World;
	if (!IsValid(LocalWorld))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(LocalWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}

AVoxelWorld* FDRSnowSurfaceEditor::ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const
{
	if (IsValid(Request.TargetVoxelWorld.Get()))
	{
		return Request.TargetVoxelWorld.Get();
	}

	UWorld* LocalWorld = this->World;
	if (!IsValid(LocalWorld))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(LocalWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}
