#include "DRSnowSurfaceEditor.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Snow/DRDirectionalSurfaceTool.h"
#include "DeepRaiders/Snow/DRSnowAbsorbTool.h"
#include "DeepRaiders/Snow/DRSnowMaterialMapping.h"
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
	const uint8 MaterialIndex)
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
		false,
		true);
	return EditedMaterialBounds.IsValid();
}

void PaintProcessedMaterialSurfaceAsync(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const uint8 MaterialIndex,
	TFunction<void()> Completion)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || ProcessedVoxels.Voxels->Num() == 0 ||
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

void AddVoxelToMaterialPatch(
	FDRSnowMaterialPatch& Patch,
	const FIntVector& VoxelPosition,
	const uint8 MaterialIndex)
{
	const FIntVector ChunkCoord = DRSnowMaterialPatchUtils::VoxelToChunkCoord(VoxelPosition);
	FDRSnowMaterialChunkPatch* ChunkPatch = Patch.Chunks.FindByPredicate(
		[ChunkCoord](const FDRSnowMaterialChunkPatch& Entry)
		{
			return Entry.ChunkCoord == ChunkCoord;
		});
	if (!ChunkPatch)
	{
		ChunkPatch = &Patch.Chunks.AddDefaulted_GetRef();
		ChunkPatch->ChunkCoord = ChunkCoord;
	}

	FDRSnowMaterialIndexSet* MaterialSet = ChunkPatch->MaterialSets.FindByPredicate(
		[MaterialIndex](const FDRSnowMaterialIndexSet& Entry)
		{
			return Entry.MaterialIndex == MaterialIndex;
		});
	if (!MaterialSet)
	{
		MaterialSet = &ChunkPatch->MaterialSets.AddDefaulted_GetRef();
		MaterialSet->MaterialIndex = MaterialIndex;
	}

	MaterialSet->LocalVoxelIndices.Add(
		DRSnowMaterialPatchUtils::VoxelToLocalIndex(VoxelPosition));
}

void SortMaterialPatch(FDRSnowMaterialPatch& Patch)
{
	Patch.Chunks.Sort([](const FDRSnowMaterialChunkPatch& A, const FDRSnowMaterialChunkPatch& B)
	{
		if (A.ChunkCoord.X != B.ChunkCoord.X)
		{
			return A.ChunkCoord.X < B.ChunkCoord.X;
		}
		if (A.ChunkCoord.Y != B.ChunkCoord.Y)
		{
			return A.ChunkCoord.Y < B.ChunkCoord.Y;
		}
		return A.ChunkCoord.Z < B.ChunkCoord.Z;
	});

	for (FDRSnowMaterialChunkPatch& Chunk : Patch.Chunks)
	{
		Chunk.MaterialSets.Sort([](const FDRSnowMaterialIndexSet& A, const FDRSnowMaterialIndexSet& B)
		{
			return A.MaterialIndex < B.MaterialIndex;
		});
		for (FDRSnowMaterialIndexSet& MaterialSet : Chunk.MaterialSets)
		{
			MaterialSet.LocalVoxelIndices.Sort();
		}
	}
}

void BuildAndValidateMaterialPatch(FDRSnowResolvedMaterialEdit& ResolvedEdit)
{
	TMap<FIntVector, uint8> ExpectedMaterials;
	for (const FDRSnowResolvedMaterialGroup& Group : ResolvedEdit.Groups)
	{
		for (const FVoxelSurfaceEditsVoxel& Voxel : *Group.ProcessedVoxels.Voxels)
		{
			ExpectedMaterials.Add(Voxel.Position, Group.MaterialIndex);
			AddVoxelToMaterialPatch(
				ResolvedEdit.MaterialPatch,
				Voxel.Position,
				Group.MaterialIndex);
		}
	}
	SortMaterialPatch(ResolvedEdit.MaterialPatch);

	TMap<FIntVector, uint8> PatchMaterials;
	int32 DuplicateOrConflictingCount = 0;
	for (const FDRSnowMaterialChunkPatch& Chunk : ResolvedEdit.MaterialPatch.Chunks)
	{
		for (const FDRSnowMaterialIndexSet& MaterialSet : Chunk.MaterialSets)
		{
			for (const uint16 LocalIndex : MaterialSet.LocalVoxelIndices)
			{
				const FIntVector Position = DRSnowMaterialPatchUtils::LocalIndexToVoxel(
					Chunk.ChunkCoord,
					LocalIndex);
				if (const uint8* ExistingMaterial = PatchMaterials.Find(Position))
				{
					if (*ExistingMaterial != MaterialSet.MaterialIndex)
					{
						++DuplicateOrConflictingCount;
					}
					continue;
				}
				PatchMaterials.Add(Position, MaterialSet.MaterialIndex);
			}
		}
	}

	int32 MismatchCount = DuplicateOrConflictingCount;
	for (const TPair<FIntVector, uint8>& Expected : ExpectedMaterials)
	{
		const uint8* PatchMaterial = PatchMaterials.Find(Expected.Key);
		if (!PatchMaterial || *PatchMaterial != Expected.Value)
		{
			++MismatchCount;
		}
	}
	for (const TPair<FIntVector, uint8>& PatchEntry : PatchMaterials)
	{
		if (!ExpectedMaterials.Contains(PatchEntry.Key))
		{
			++MismatchCount;
		}
	}

	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/VoxelCount"), ResolvedEdit.MaterialPatch.NumVoxels());
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/ChunkCount"), ResolvedEdit.MaterialPatch.Chunks.Num());
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/EstimatedBytes"), ResolvedEdit.MaterialPatch.EstimateSerializedBytes());
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/MaterialMismatchCount"), MismatchCount);
	ensureMsgf(
		MismatchCount == 0,
		TEXT("Snow material patch shadow validation failed with %d mismatched voxel(s)"),
		MismatchCount);
}

void ResolveProcessedSnowMaterials(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore,
	const bool bUseSurfacePositionForVolume,
	FDRSnowResolvedMaterialEdit& OutResolvedEdit)
{
	TArray<FIntVector> Positions;
	Positions.Reserve(ProcessedVoxels.Voxels->Num());
	for (const FVoxelSurfaceEditsVoxel& Voxel : *ProcessedVoxels.Voxels)
	{
		Positions.Add(Voxel.Position);
	}

	TArray<uint8> ResolvedMaterialIndices;
	TBitArray<> FoundOwnership;
	OwnershipStore.ResolveNearestMaterialIndicesAtVoxels(
		VoxelWorld,
		Positions,
		2,
		ResolvedMaterialIndices,
		FoundOwnership);

	TMap<uint8, TArray<FVoxelSurfaceEditsVoxel>> VoxelsByMaterial;
	for (int32 VoxelIndex = 0; VoxelIndex < ProcessedVoxels.Voxels->Num(); ++VoxelIndex)
	{
		const FVoxelSurfaceEditsVoxel& Voxel = (*ProcessedVoxels.Voxels)[VoxelIndex];
		uint8 MaterialIndex = ResolvedMaterialIndices[VoxelIndex];
		if (!FoundOwnership[VoxelIndex])
		{
			const FVector SampleWorldLocation =
				bUseSurfacePositionForVolume && ProcessedVoxels.Info.bHasSurfacePositions
					? VoxelWorld->LocalToGlobalFloat(FVoxelVector(Voxel.SurfacePosition))
					: VoxelWorld->LocalToGlobal(Voxel.Position);
			const int32 DominantTeamId = VolumeStore.GetDominantTeamAtLocation(SampleWorldLocation);
			MaterialIndex = DRSnowMaterialMapping::TeamToMaterialIndex(DominantTeamId);
		}

		VoxelsByMaterial.FindOrAdd(MaterialIndex).Add(Voxel);
	}

	OutResolvedEdit = FDRSnowResolvedMaterialEdit();
	OutResolvedEdit.VoxelWorld = VoxelWorld;
	OutResolvedEdit.Groups.Reserve(VoxelsByMaterial.Num());
	for (TPair<uint8, TArray<FVoxelSurfaceEditsVoxel>>& MaterialVoxels : VoxelsByMaterial)
	{
		if (MaterialVoxels.Value.IsEmpty())
		{
			continue;
		}

		FDRSnowResolvedMaterialGroup& Group = OutResolvedEdit.Groups.AddDefaulted_GetRef();
		Group.MaterialIndex = MaterialVoxels.Key;
		Group.ProcessedVoxels = MakeProcessedVoxelGroup(
			ProcessedVoxels,
			MoveTemp(MaterialVoxels.Value));
	}
	OutResolvedEdit.Groups.Sort([](const FDRSnowResolvedMaterialGroup& A, const FDRSnowResolvedMaterialGroup& B)
	{
		return A.MaterialIndex < B.MaterialIndex;
	});
	BuildAndValidateMaterialPatch(OutResolvedEdit);
}


}

FDRSnowSurfaceEditResult FDRSnowSurfaceEditor::AddSnowAtArea(
	const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowSurfaceEditResult Result;
	const bool bUsesOrientedBox = Request.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool;
	if (Request.Amount <= 0.f ||
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

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		// Directional 도구의 실제 변경 목록은 Subsystem이 Ownership/Volume 원본 데이터를 갱신할 때 사용한다.
		FVoxelSurfaceEditsProcessedVoxels SurfaceFootprint;
		if (!Request.bUseVirtualSurface)
		{
			SurfaceFootprint =
				UDRDirectionalSurfaceTool::FindSurfaceFootprint(
					VoxelWorld,
					Request.WorldLocation,
					Request.Radius,
					SnowSurfaceFalloff,
					Request.Amount,
					true);
		}
		if (Request.bUseVirtualSurface ||
			(Request.bAllowVirtualSurfaceFallback && SurfaceFootprint.Voxels->Num() == 0))
		{
			SurfaceFootprint =
				UDRDirectionalSurfaceTool::MakeVirtualSurfaceFootprint(
					VoxelWorld,
					Request.WorldLocation,
					Request.SurfaceNormal,
					Request.Radius,
					SnowSurfaceFalloff,
					Request.Amount,
					true);
		}

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
				PaintProcessedMaterialSurface(
					VoxelWorld,
					UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
						EditedBounds,
						ModifiedValues,
						true),
					DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
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
				PaintProcessedMaterialSurface(
					VoxelWorld,
					MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
					DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
			}

		}

		Result.VoxelWorld = VoxelWorld;
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
		(Request.bAllowVirtualSurfaceFallback && SurfaceFootprint.Voxels->Num() == 0))
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

bool FDRSnowSurfaceEditor::ResolveSnowMaterialsAtArea(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore,
	FDRSnowResolvedMaterialEdit& OutResolvedEdit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_ResolveArea);
	OutResolvedEdit = FDRSnowResolvedMaterialEdit();
	if (!EditResult.EditedBounds.IsValid() || EditResult.ModifiedValues.IsEmpty())
	{
		return false;
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Repaint/ModifiedInput"), EditResult.ModifiedValues.Num());

	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!IsValid(VoxelWorld))
	{
		VoxelWorld = ResolveVoxelWorld(Request);
	}
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	constexpr int32 RepaintNeighborRadius = 2;
	const FVoxelIntBox SurfaceBounds = EditResult.EditedBounds.Extend(RepaintNeighborRadius);
	if (!SurfaceBounds.IsValid() || SurfaceBounds.Count() > static_cast<uint64>(MAX_int32))
	{
		return false;
	}

	const FIntVector BoundsSize = SurfaceBounds.Size();
	const int32 BoundsVoxelCount = static_cast<int32>(SurfaceBounds.Count());
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Repaint/BoundsVoxels"), BoundsVoxelCount);
	TBitArray<> AffectedPositions(false, BoundsVoxelCount);
	auto GetAffectedIndex = [SurfaceBounds, BoundsSize](const FIntVector& Position)
	{
		const FIntVector LocalPosition = Position - SurfaceBounds.Min;
		return LocalPosition.X + BoundsSize.X * (LocalPosition.Y + BoundsSize.Y * LocalPosition.Z);
	};

	bool bHasAffectedPosition = false;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_BuildAffectedMask);
		for (const FModifiedVoxelValue& ModifiedValue : EditResult.ModifiedValues)
		{
			if (ModifiedValue.NewValue <= ModifiedValue.OldValue)
			{
				continue;
			}

			for (int32 Z = -RepaintNeighborRadius; Z <= RepaintNeighborRadius; ++Z)
			{
				for (int32 Y = -RepaintNeighborRadius; Y <= RepaintNeighborRadius; ++Y)
				{
					for (int32 X = -RepaintNeighborRadius; X <= RepaintNeighborRadius; ++X)
					{
						const FIntVector Position = ModifiedValue.Position + FIntVector(X, Y, Z);
						if (!SurfaceBounds.Contains(Position))
						{
							continue;
						}
						AffectedPositions[GetAffectedIndex(Position)] = true;
						bHasAffectedPosition = true;
					}
				}
			}
		}
	}
	if (!bHasAffectedPosition)
	{
		return false;
	}

	FVoxelSurfaceEditsVoxels SurfaceVoxels;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_QuerySurface);
		UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(
			SurfaceVoxels,
			VoxelWorld,
			SurfaceBounds,
			true);
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Repaint/SurfaceQueried"), SurfaceVoxels.Voxels->Num());

	TArray<FVoxelSurfaceEditsVoxel> RepaintVoxels;
	RepaintVoxels.Reserve(SurfaceVoxels.Voxels->Num());
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_FilterAffectedSurface);
		for (const FVoxelSurfaceEditsVoxelBase& SourceVoxel : *SurfaceVoxels.Voxels)
		{
			if (!SurfaceBounds.Contains(SourceVoxel.Position) ||
				!AffectedPositions[GetAffectedIndex(SourceVoxel.Position)])
			{
				continue;
			}

			FVoxelSurfaceEditsVoxel& RepaintVoxel =
				RepaintVoxels.Add_GetRef(FVoxelSurfaceEditsVoxel(SourceVoxel));
			RepaintVoxel.Strength = 1.f;
		}
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Repaint/FilteredVoxels"), RepaintVoxels.Num());
	if (RepaintVoxels.IsEmpty())
	{
		return false;
	}

	FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
	ProcessedVoxels.Bounds = SurfaceBounds;
	ProcessedVoxels.Info = SurfaceVoxels.Info;
	ProcessedVoxels.Voxels =
		MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(RepaintVoxels));

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_ResolveOwnership);
		ResolveProcessedSnowMaterials(
			VoxelWorld,
			ProcessedVoxels,
			OwnershipStore,
			VolumeStore,
			true,
			OutResolvedEdit);
	}
	return !OutResolvedEdit.IsEmpty();
}

bool FDRSnowSurfaceEditor::ResolveSnowMaterialsAtModifiedVoxels(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore,
	FDRSnowResolvedMaterialEdit& OutResolvedEdit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_ResolveModified);
	OutResolvedEdit = FDRSnowResolvedMaterialEdit();
	if (!EditResult.EditedBounds.IsValid() || EditResult.ModifiedValues.IsEmpty())
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!IsValid(VoxelWorld))
	{
		VoxelWorld = ResolveVoxelWorld(Request);
	}
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	const FVoxelSurfaceEditsProcessedVoxels DirtyVoxels =
		UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
			EditResult.EditedBounds,
			EditResult.ModifiedValues,
			false);
	if (DirtyVoxels.Voxels->IsEmpty())
	{
		return false;
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Repaint/DirtyVoxels"), DirtyVoxels.Voxels->Num());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_Dirty_ResolveOwnership);
		ResolveProcessedSnowMaterials(
			VoxelWorld,
			DirtyVoxels,
			OwnershipStore,
			VolumeStore,
			false,
			OutResolvedEdit);
	}
	return !OutResolvedEdit.IsEmpty();
}

bool FDRSnowSurfaceEditor::ApplyResolvedSnowMaterials(
	const FDRSnowResolvedMaterialEdit& ResolvedEdit)
{
	AVoxelWorld* VoxelWorld = ResolvedEdit.VoxelWorld.Get();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	bool bPaintedAny = false;
	for (const FDRSnowResolvedMaterialGroup& Group : ResolvedEdit.Groups)
	{
		bPaintedAny |= PaintProcessedMaterialSurface(
			VoxelWorld,
			Group.ProcessedVoxels,
			Group.MaterialIndex);
	}
	return bPaintedAny;
}

bool FDRSnowSurfaceEditor::ApplySnowMaterialPatch(
	AVoxelWorld* VoxelWorld,
	const FDRSnowMaterialPatch& MaterialPatch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_ApplyMaterialPatch);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB || MaterialPatch.IsEmpty())
	{
		return false;
	}

	bool bPaintedAny = false;
	int32 MissingSurfaceVoxelCount = 0;
	for (const FDRSnowMaterialChunkPatch& ChunkPatch : MaterialPatch.Chunks)
	{
		TMap<uint16, uint8> MaterialByLocalIndex;
		for (const FDRSnowMaterialIndexSet& MaterialSet : ChunkPatch.MaterialSets)
		{
			for (const uint16 LocalIndex : MaterialSet.LocalVoxelIndices)
			{
				MaterialByLocalIndex.Add(LocalIndex, MaterialSet.MaterialIndex);
			}
		}
		if (MaterialByLocalIndex.IsEmpty())
		{
			continue;
		}

		const FIntVector ChunkMin = ChunkPatch.ChunkCoord * DRSnowMaterialPatchUtils::ChunkSize;
		const FVoxelIntBox ChunkBounds(
			ChunkMin,
			ChunkMin + FIntVector(DRSnowMaterialPatchUtils::ChunkSize));
		FVoxelSurfaceEditsVoxels SurfaceVoxels;
		UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(
			SurfaceVoxels,
			VoxelWorld,
			ChunkBounds,
			true);

		TMap<uint8, TArray<FVoxelSurfaceEditsVoxel>> VoxelsByMaterial;
		for (const FVoxelSurfaceEditsVoxelBase& SourceVoxel : *SurfaceVoxels.Voxels)
		{
			if (!ChunkBounds.Contains(SourceVoxel.Position))
			{
				continue;
			}
			const uint16 LocalIndex = DRSnowMaterialPatchUtils::VoxelToLocalIndex(SourceVoxel.Position);
			const uint8* MaterialIndex = MaterialByLocalIndex.Find(LocalIndex);
			if (!MaterialIndex)
			{
				continue;
			}

			FVoxelSurfaceEditsVoxel& Voxel = VoxelsByMaterial.FindOrAdd(*MaterialIndex).Add_GetRef(
				FVoxelSurfaceEditsVoxel(SourceVoxel));
			Voxel.Strength = 1.f;
			MaterialByLocalIndex.Remove(LocalIndex);
		}
		MissingSurfaceVoxelCount += MaterialByLocalIndex.Num();

		FVoxelSurfaceEditsProcessedVoxels ProcessedVoxels;
		ProcessedVoxels.Bounds = ChunkBounds;
		ProcessedVoxels.Info = SurfaceVoxels.Info;
		for (TPair<uint8, TArray<FVoxelSurfaceEditsVoxel>>& MaterialVoxels : VoxelsByMaterial)
		{
			ProcessedVoxels.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(
				MoveTemp(MaterialVoxels.Value));
			bPaintedAny |= PaintProcessedMaterialSurface(
				VoxelWorld,
				ProcessedVoxels,
				MaterialVoxels.Key);
		}
	}
	TRACE_UNCHECKED_INT_VALUE(TEXT("DRSnow/Patch/MissingSurfaceVoxelCount"), MissingSurfaceVoxelCount);
	return bPaintedAny;
}

bool FDRSnowSurfaceEditor::RepaintSnowMaterialsAtArea(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_Total);
	FDRSnowResolvedMaterialEdit ResolvedEdit;
	return ResolveSnowMaterialsAtArea(
			Request,
			EditResult,
			OwnershipStore,
			VolumeStore,
			ResolvedEdit) &&
		ApplyResolvedSnowMaterials(ResolvedEdit);
}

bool FDRSnowSurfaceEditor::RepaintSnowMaterialsAtModifiedVoxels(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_Dirty_Total);
	FDRSnowResolvedMaterialEdit ResolvedEdit;
	return ResolveSnowMaterialsAtModifiedVoxels(
			Request,
			EditResult,
			OwnershipStore,
			VolumeStore,
			ResolvedEdit) &&
		ApplyResolvedSnowMaterials(ResolvedEdit);
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
