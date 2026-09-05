#include "DRSnowSurfaceEditor.h"

#include "DeepRaiders/Snow/DRDirectionalSurfaceTool.h"
#include "DeepRaiders/Snow/DRSnowAbsorbTool.h"
#include "DeepRaiders/Snow/DRSnowReplicationTypes.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "EngineUtils.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelTools/Gen/VoxelBoxTools.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelPaintMaterial.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelMaterial.h"
#include "VoxelWorld.h"

namespace
{
constexpr float SnowSurfaceDistanceDivisor = 4.f;
constexpr float SnowSurfaceFalloff = 0.35f;

FVoxelPaintMaterial MakeIndexPaintMaterial(
	const EVoxelMaterialConfig MaterialConfig,
	const uint8 MaterialIndex)
{
	FVoxelPaintMaterial PaintMaterial;
	if (MaterialConfig == EVoxelMaterialConfig::SingleIndex)
	{
		PaintMaterial.Type = EVoxelPaintMaterialType::SingleIndex;
		PaintMaterial.SingleIndex.Channel.Channel = MaterialIndex;
	}
	else if (MaterialConfig == EVoxelMaterialConfig::MultiIndex)
	{
		PaintMaterial.Type = EVoxelPaintMaterialType::MultiIndex;
		PaintMaterial.SingleIndex.Channel.Channel = MaterialIndex;
		PaintMaterial.MultiIndex.TargetValue = 1.f;
	}

	return PaintMaterial;
}

FDRSnowSurfaceEditResult AddOrientedBoxSnow(
	AVoxelWorld* VoxelWorld,
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowSurfaceEditResult Result;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	const FTransform BoxTransform(Request.BoxRotation, Request.WorldLocation);
	FBox LocalBounds(ForceInit);
	for (int32 XSign : {-1, 1})
	{
		for (int32 YSign : {-1, 1})
		{
			for (int32 ZSign : {-1, 1})
			{
				const FVector WorldCorner = BoxTransform.TransformPosition(FVector(
					Request.BoxExtent.X * XSign,
					Request.BoxExtent.Y * YSign,
					Request.BoxExtent.Z * ZSign));
				const FVector LocalCorner = VoxelWorld->GlobalToLocalFloat(WorldCorner).ToFloat();
				if (LocalBounds.IsValid)
				{
					LocalBounds.Min = LocalBounds.Min.ComponentMin(LocalCorner);
					LocalBounds.Max = LocalBounds.Max.ComponentMax(LocalCorner);
				}
				else
				{
					LocalBounds = FBox(LocalCorner, LocalCorner);
				}
			}
		}
	}

	const FVoxelIntBox CandidateBounds(LocalBounds.Min, LocalBounds.Max);
	if (!CandidateBounds.IsValid())
	{
		return Result;
	}

	FVoxelMaterial SnowMaterial;
	SnowMaterial.SetSingleIndex(DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelWriteScopeLock Lock(Data, CandidateBounds, FUNCTION_FNAME);
		for (int32 Z = CandidateBounds.Min.Z; Z < CandidateBounds.Max.Z; ++Z)
		{
			for (int32 Y = CandidateBounds.Min.Y; Y < CandidateBounds.Max.Y; ++Y)
			{
				for (int32 X = CandidateBounds.Min.X; X < CandidateBounds.Max.X; ++X)
				{
					const FIntVector VoxelPosition(X, Y, Z);
					const FVector WorldPosition = VoxelWorld->LocalToGlobalFloat(FVector(VoxelPosition));
					const FVector BoxLocalPosition = BoxTransform.InverseTransformPosition(WorldPosition);
					if (FMath::Abs(BoxLocalPosition.X) > Request.BoxExtent.X ||
						FMath::Abs(BoxLocalPosition.Y) > Request.BoxExtent.Y ||
						FMath::Abs(BoxLocalPosition.Z) > Request.BoxExtent.Z)
					{
						continue;
					}

					if (!Data.GetValue(VoxelPosition, 0).IsEmpty())
					{
						continue;
					}

					const FVoxelValue OldValue = Data.GetValue(VoxelPosition, 0);
					const FVoxelValue NewValue = FVoxelValue::Full();
					Data.SetValue(VoxelPosition, NewValue);
					Data.SetMaterial(VoxelPosition, SnowMaterial);
					ModifiedValues.Emplace(VoxelPosition, OldValue, NewValue);
				}
			}
		}
	}

	if (!ModifiedValues.IsEmpty())
	{
		Result.AppliedAmount = Request.Amount;
		Result.VoxelWorld = VoxelWorld;
		Result.EditedBounds = CandidateBounds;
		Result.ModifiedValues = MoveTemp(ModifiedValues);
		UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, CandidateBounds.Extend(1));
	}

	return Result;
}

bool PaintProcessedMaterialSurface(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const uint8 MaterialIndex,
	TArray<FModifiedVoxelMaterial>* OutModifiedMaterials = nullptr)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_EditMaterials);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || ProcessedVoxels.Voxels->Num() == 0 ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB)
	{
		return false;
	}

	TArray<FModifiedVoxelMaterial> ModifiedMaterials;
	FVoxelIntBox EditedMaterialBounds;
	UVoxelSurfaceEditTools::EditVoxelMaterials(
		ModifiedMaterials,
		EditedMaterialBounds,
		VoxelWorld,
		MakeIndexPaintMaterial(VoxelWorld->MaterialConfig, MaterialIndex),
		ProcessedVoxels,
		true,
		OutModifiedMaterials != nullptr,
		true);
	if (OutModifiedMaterials)
	{
		OutModifiedMaterials->Append(MoveTemp(ModifiedMaterials));
	}
	return EditedMaterialBounds.IsValid();
}

void PaintProcessedMaterialSurfaceAsync(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const uint8 MaterialIndex,
	TFunction<void()> Completion)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || ProcessedVoxels.Voxels->IsEmpty() ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB)
	{
		Completion();
		return;
	}

	UVoxelSurfaceEditTools::EditVoxelMaterialsAsync(
		VoxelWorld,
		MakeIndexPaintMaterial(VoxelWorld->MaterialConfig, MaterialIndex),
		ProcessedVoxels,
		FOnVoxelToolComplete_WithModifiedMaterials::CreateLambda(
			[Completion = MoveTemp(Completion)](const TArray<FModifiedVoxelMaterial>&) mutable
			{
				Completion();
			}),
		nullptr,
		true,
		false,
		false);
}

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

struct FDRSnowMaterialPatchAsyncState : TSharedFromThis<FDRSnowMaterialPatchAsyncState>
{
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;
	FDRSnowMaterialPatch Patch;
	TFunction<void(bool, TArray<FVoxelIntBox>&&)> Completion;
	TArray<FVoxelIntBox> EditedChunkBounds;
	int32 ChunkIndex = 0;
	int32 MaterialSetIndex = 0;
	bool bEditedCurrentChunk = false;
	bool bAppliedAny = false;
	bool bSynchronous = false;

	void ProcessNextMaterialSet()
	{
		AVoxelWorld* World = VoxelWorld.Get();
		if (!IsValid(World) || !World->IsCreated())
		{
			Finish();
			return;
		}

		while (ChunkIndex < Patch.Chunks.Num())
		{
			const FDRSnowMaterialChunkPatch& Chunk = Patch.Chunks[ChunkIndex];
			if (MaterialSetIndex >= Chunk.MaterialSets.Num())
			{
				if (bEditedCurrentChunk)
				{
					EditedChunkBounds.Add(GetChunkBounds(Chunk.ChunkCoord));
				}
				++ChunkIndex;
				MaterialSetIndex = 0;
				bEditedCurrentChunk = false;
				continue;
			}

			const FDRSnowMaterialIndexSet& MaterialSet = Chunk.MaterialSets[MaterialSetIndex];
			FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels =
				MakeProcessedVoxels(Chunk.ChunkCoord, MaterialSet.LocalVoxelIndices);
			if (ProcessedVoxels.Voxels->IsEmpty())
			{
				++MaterialSetIndex;
				continue;
			}

			if (bSynchronous)
			{
				TArray<FModifiedVoxelMaterial> ModifiedMaterials;
				FVoxelIntBox EditedBounds;
				UVoxelSurfaceEditTools::EditVoxelMaterials(
					ModifiedMaterials,
					EditedBounds,
					World,
					MakeIndexPaintMaterial(World->MaterialConfig, MaterialSet.MaterialIndex),
					ProcessedVoxels,
					false,
					false,
					false);
				bAppliedAny = true;
				bEditedCurrentChunk = true;
				++MaterialSetIndex;
				continue;
			}

			const TSharedRef<FDRSnowMaterialPatchAsyncState> State = AsShared();
			UVoxelSurfaceEditTools::EditVoxelMaterialsAsync(
				World,
				MakeIndexPaintMaterial(World->MaterialConfig, MaterialSet.MaterialIndex),
				ProcessedVoxels,
				FOnVoxelToolComplete_WithModifiedMaterials::CreateLambda(
					[State](const TArray<FModifiedVoxelMaterial>&)
					{
						State->bAppliedAny = true;
						State->bEditedCurrentChunk = true;
						++State->MaterialSetIndex;
						State->ProcessNextMaterialSet();
					}),
				nullptr,
				false,
				false,
				false);
			return;
		}

		Finish();
	}

private:
	static FVoxelIntBox GetChunkBounds(const FIntVector& ChunkCoord)
	{
		const FIntVector ChunkMin = ChunkCoord * DRSnowMaterialPatchUtils::ChunkSize;
		return FVoxelIntBox(
			ChunkMin,
			ChunkMin + FIntVector(DRSnowMaterialPatchUtils::ChunkSize));
	}

	static FVoxelSurfaceEditsProcessedVoxels MakeProcessedVoxels(
		const FIntVector& ChunkCoord,
		const TArray<uint16>& LocalVoxelIndices)
	{
		constexpr int32 MaxLocalIndex =
			DRSnowMaterialPatchUtils::ChunkSize *
			DRSnowMaterialPatchUtils::ChunkSize *
			DRSnowMaterialPatchUtils::ChunkSize;

		TArray<FVoxelSurfaceEditsVoxel> Voxels;
		Voxels.Reserve(LocalVoxelIndices.Num());
		for (const uint16 LocalIndex : LocalVoxelIndices)
		{
			if (LocalIndex >= MaxLocalIndex)
			{
				continue;
			}

			FVoxelSurfaceEditsVoxel& Voxel = Voxels.AddDefaulted_GetRef();
			Voxel.Position = DRSnowMaterialPatchUtils::LocalIndexToVoxel(ChunkCoord, LocalIndex);
			Voxel.Strength = 1.f;
		}

		FVoxelSurfaceEditsProcessedVoxels Result;
		Result.Bounds = GetChunkBounds(ChunkCoord);
		Result.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(Voxels));
		return Result;
	}

	void Finish()
	{
		if (Completion)
		{
			Completion(bAppliedAny, MoveTemp(EditedChunkBounds));
		}
	}
};


}

FDRSnowSurfaceEditResult FDRSnowSurfaceEditor::AddSnowAtArea(
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowSurfaceEditResult Result;
	const bool bUsesOrientedBox = Request.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool;
	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool || Request.Amount <= 0.f ||
		(bUsesOrientedBox && (Request.BoxExtent.X <= 0.f || Request.BoxExtent.Y <= 0.f || Request.BoxExtent.Z <= 0.f)) ||
		(!bUsesOrientedBox && Request.Radius <= 0.f))
	{
		return Result;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	if (bUsesOrientedBox)
	{
		return AddOrientedBoxSnow(VoxelWorld, Request);
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
				PaintProcessedMaterialSurface(
					VoxelWorld,
					MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
					DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
			}

		}

		Result.VoxelWorld = VoxelWorld;
		Result.EditedBounds = EditedBounds;
		Result.ModifiedValues = MoveTemp(ModifiedValues);
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
		PaintProcessedMaterialSurface(
			VoxelWorld,
			MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
			DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));

	}

	Result.VoxelWorld = VoxelWorld;
	Result.EditedBounds = EditedBounds;
	Result.ModifiedValues = MoveTemp(ModifiedValues);
	return Result;
}

bool FDRSnowSurfaceEditor::AddDirectionalSnowAtAreaAsync(
	const FDRSnowSurfaceAddRequest& Request,
	TFunction<void(FDRSnowSurfaceEditResult&&)> Completion)
{
	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool ||
		Request.Radius <= 0.f || Request.Amount <= 0.f || !Completion)
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FVoxelSurfaceEditsProcessedVoxels SurfaceFootprint;
	if (!Request.bUseVirtualSurface)
	{
		SurfaceFootprint = UDRDirectionalSurfaceTool::FindSurfaceFootprint(
			VoxelWorld,
			Request.WorldLocation,
			Request.Radius,
			SnowSurfaceFalloff,
			Request.Amount,
			true);
	}
	if (Request.bUseVirtualSurface ||
		(Request.bAllowVirtualSurfaceFallback && SurfaceFootprint.Voxels->IsEmpty()))
	{
		SurfaceFootprint = UDRDirectionalSurfaceTool::MakeVirtualSurfaceFootprint(
			VoxelWorld,
			Request.WorldLocation,
			Request.SurfaceNormal,
			Request.Radius,
			SnowSurfaceFalloff,
			Request.Amount,
			true);
	}

	const TWeakObjectPtr<AVoxelWorld> WeakVoxelWorld = VoxelWorld;
	return UDRDirectionalSurfaceTool::ApplySurfaceVolumeEditAsync(
		VoxelWorld,
		SurfaceFootprint,
		SnowSurfaceDistanceDivisor,
		true,
		[WeakVoxelWorld, Request, Completion = MoveTemp(Completion)](
			TArray<FModifiedVoxelValue>&& ModifiedValues,
			FVoxelIntBox EditedBounds) mutable
		{
			FDRSnowSurfaceEditResult Result;
			AVoxelWorld* ValidVoxelWorld = WeakVoxelWorld.Get();
			if (!IsValid(ValidVoxelWorld) || !ValidVoxelWorld->IsCreated())
			{
				Completion(MoveTemp(Result));
				return;
			}

			Result.AppliedAmount = FMath::Min(Request.Amount, GetModifiedValueAmount(ModifiedValues));
			if (Result.AppliedAmount <= 0.f || !EditedBounds.IsValid())
			{
				Completion(MoveTemp(Result));
				return;
			}

			const FVoxelSurfaceEditsProcessedVoxels PaintVoxels =
				UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
					EditedBounds,
					ModifiedValues,
					true);
			Result.VoxelWorld = ValidVoxelWorld;
			Result.EditedBounds = EditedBounds;
			Result.ModifiedValues = MoveTemp(ModifiedValues);
			Result.bUseModifiedValuesForVolume = true;

			PaintProcessedMaterialSurfaceAsync(
				ValidVoxelWorld,
				PaintVoxels,
				DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId),
				[Result = MoveTemp(Result), Completion = MoveTemp(Completion)]() mutable
				{
					if (AVoxelWorld* PaintedVoxelWorld = Result.VoxelWorld.Get();
						IsValid(PaintedVoxelWorld) && PaintedVoxelWorld->IsCreated() &&
						Result.EditedBounds.IsValid())
					{
						// Density와 team material이 모두 반영된 뒤 한 번만 렌더를 갱신한다.
						UVoxelBlueprintLibrary::UpdateBounds(
							PaintedVoxelWorld,
							Result.EditedBounds.Extend(1));
					}
					Completion(MoveTemp(Result));
				});
		});
}

FDRSnowSurfaceEditResult FDRSnowSurfaceEditor::RemoveSnowWithAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_SurfaceEdit_Total);
	FDRSnowSurfaceEditResult Result;
	if (Request.RemovalMode != EDRSnowRemovalMode::AbsorbTool ||
		Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return Result;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelIntBox EditedBounds;
	float ModifiedValueAmount = 0.f;
	if (Request.bUseAdaptiveAbsorbQuery)
	{
		ModifiedValueAmount = UDRSnowAbsorbTool::RemoveSnowFromFrustumAdaptive(
			VoxelWorld,
			Request.BrushOrigin,
			Request.WorldLocation,
			Request.Radius,
			FMath::Clamp(Request.AbsorbInnerRadiusRatio, 0.f, 1.f),
			0.2f,
			Request.RequestedAmount,
			SnowSurfaceDistanceDivisor,
			Request.AbsorbSweepRadius,
			Request.AbsorbMaxSweepsPerTick,
			ModifiedValues,
			EditedBounds);
	}
	else
	{
		ModifiedValueAmount = UDRSnowAbsorbTool::RemoveSnowFromFrustum(
			VoxelWorld,
			Request.BrushOrigin,
			Request.WorldLocation,
			Request.Radius,
			FMath::Clamp(Request.AbsorbInnerRadiusRatio, 0.f, 1.f),
			0.2f,
			Request.RequestedAmount,
			SnowSurfaceDistanceDivisor,
			ModifiedValues,
			EditedBounds);
	}

	Result.AppliedAmount = FMath::Min(Request.RequestedAmount, ModifiedValueAmount);
	if (Result.AppliedAmount > 0.f)
	{
		Result.VoxelWorld = VoxelWorld;
		Result.EditedBounds = EditedBounds;
		Result.ModifiedValues = MoveTemp(ModifiedValues);
		Result.bUseModifiedValuesForVolume = true;
	}
	return Result;
}

FDRSnowSurfaceEditResult FDRSnowSurfaceEditor::RemoveSnowAtArea(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	FDRSnowSurfaceEditResult Result;
	if (Request.RemovalMode == EDRSnowRemovalMode::AbsorbTool ||
		Request.Radius <= 0.f || Request.RequestedAmount <= 0.f)
	{
		return Result;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Request);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	// 일반 아이템과 상호작용은 Sphere/Box 기반 제거만 처리한다.
	TArray<FModifiedVoxelValue> ModifiedValues;
	FVoxelIntBox EditedBounds;

	FVector BrushCenter = Request.WorldLocation;
	if (Request.RemovalMode == EDRSnowRemovalMode::ContactBrush)
	{
		const FVector TowardTarget = (Request.WorldLocation - Request.BrushOrigin).GetSafeNormal();
		const FVector InwardDirection = TowardTarget.IsNearlyZero()
			? -Request.SurfaceNormal.GetSafeNormal()
			: TowardTarget;
		if (InwardDirection.IsNearlyZero())
		{
			return Result;
		}

		const float PenetrationDepth = FMath::Min(
			Request.Radius,
			FMath::Clamp(
				VoxelWorld->VoxelSize * Request.RequestedAmount,
				VoxelWorld->VoxelSize * 0.5f,
				VoxelWorld->VoxelSize * 2.f));
		const float ShapeSupportDistance =
			Request.RemovalBrushShape == EDRSnowRemovalBrushShape::Box
				? Request.Radius * (
					FMath::Abs(InwardDirection.X) +
					FMath::Abs(InwardDirection.Y) +
					FMath::Abs(InwardDirection.Z))
				: Request.Radius;
		BrushCenter =
			Request.WorldLocation - InwardDirection * (ShapeSupportDistance - PenetrationDepth);
	}

	if (Request.RemovalBrushShape == EDRSnowRemovalBrushShape::Box)
	{
		const FVoxelIntBox BoxBounds = UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			BrushCenter,
			Request.Radius);
		UVoxelBoxTools::RemoveBox(
			ModifiedValues,
			EditedBounds,
			VoxelWorld,
			BoxBounds,
			false,
			true,
			true);
	}
	else
	{
		UVoxelSphereTools::RemoveSphere(
			ModifiedValues,
			EditedBounds,
			VoxelWorld,
			BrushCenter,
			Request.Radius,
			false,
			true,
			true,
			true);
	}

	const float ModifiedValueAmount = GetModifiedValueAmount(ModifiedValues);

	Result.AppliedAmount = FMath::Min(Request.RequestedAmount, ModifiedValueAmount);
	if (Result.AppliedAmount > 0.f)
	{
		Result.VoxelWorld = VoxelWorld;
		Result.EditedBounds = EditedBounds;
		Result.ModifiedValues = MoveTemp(ModifiedValues);
		Result.bUseModifiedValuesForVolume = true;
	}

	return Result;
}

bool FDRSnowSurfaceEditor::ApplyResolvedSnowMaterials(
	const FDRSnowResolvedMaterialEdit& ResolvedEdit,
	FDRSnowMaterialPatch* OutMaterialPatch)
{
	if (OutMaterialPatch)
	{
		*OutMaterialPatch = FDRSnowMaterialPatch();
	}

	AVoxelWorld* VoxelWorld = ResolvedEdit.VoxelWorld.Get();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	bool bPaintedAny = false;
	FDRSnowMaterialPatchBuilder PatchBuilder;
	for (const FDRSnowResolvedMaterialGroup& Group : ResolvedEdit.Groups)
	{
		TArray<FModifiedVoxelMaterial> ModifiedMaterials;
		const bool bPaintedGroup = PaintProcessedMaterialSurface(
			VoxelWorld,
			Group.ProcessedVoxels,
			Group.MaterialIndex,
			OutMaterialPatch ? &ModifiedMaterials : nullptr);
		bPaintedAny |= bPaintedGroup;
		if (OutMaterialPatch && bPaintedGroup)
		{
			PatchBuilder.AddChangedMaterials(Group.MaterialIndex, ModifiedMaterials);
		}
	}

	if (OutMaterialPatch)
	{
		*OutMaterialPatch = PatchBuilder.Build();
		TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/VoxelCount"), OutMaterialPatch->NumVoxels());
		TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/ChunkCount"), OutMaterialPatch->Chunks.Num());
		TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/EstimatedBytes"), OutMaterialPatch->EstimateSerializedBytes());
	}
	return bPaintedAny;
}

bool FDRSnowSurfaceEditor::ApplySnowMaterialPatchSync(
	AVoxelWorld* VoxelWorld,
	FDRSnowMaterialPatch MaterialPatch,
	TArray<FVoxelIntBox>& OutEditedChunkBounds)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_ApplyMaterialPatchSync);
	OutEditedChunkBounds.Reset();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB || MaterialPatch.IsEmpty())
	{
		return false;
	}

	TSharedRef<FDRSnowMaterialPatchAsyncState> State = MakeShared<FDRSnowMaterialPatchAsyncState>();
	State->VoxelWorld = VoxelWorld;
	State->Patch = MoveTemp(MaterialPatch);
	State->bSynchronous = true;
	State->ProcessNextMaterialSet();
	OutEditedChunkBounds = MoveTemp(State->EditedChunkBounds);
	return State->bAppliedAny;
}

bool FDRSnowSurfaceEditor::ApplySnowMaterialPatchAsync(
	AVoxelWorld* VoxelWorld,
	FDRSnowMaterialPatch MaterialPatch,
	TFunction<void(bool, TArray<FVoxelIntBox>&&)> Completion)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_QueueMaterialPatchAsync);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB || MaterialPatch.IsEmpty())
	{
		return false;
	}

	TSharedRef<FDRSnowMaterialPatchAsyncState> State = MakeShared<FDRSnowMaterialPatchAsyncState>();
	State->VoxelWorld = VoxelWorld;
	State->Patch = MoveTemp(MaterialPatch);
	State->Completion = MoveTemp(Completion);
	State->ProcessNextMaterialSet();
	return true;
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
