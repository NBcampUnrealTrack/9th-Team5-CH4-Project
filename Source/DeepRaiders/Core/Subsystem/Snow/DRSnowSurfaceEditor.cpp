#include "DRSnowSurfaceEditor.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Snow/DRDirectionalSurfaceTool.h"
#include "DeepRaiders/Snow/DRSnowAbsorbTool.h"
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

int32 GetTeamMaterialIndex(const int32 TeamId) { return TeamId == INDEX_NONE ? 0 : FMath::Max(0, TeamId) + 1; }

FVoxelPaintMaterial MakeTeamPaintMaterial(const EVoxelMaterialConfig MaterialConfig, const int32 TeamId)
{
	FVoxelPaintMaterial PaintMaterial;
	const int32 MaterialIndex = GetTeamMaterialIndex(TeamId);
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
	SnowMaterial.SetSingleIndex(GetTeamMaterialIndex(Request.Context.TeamId));
	int32 ModifiedVoxelCount = 0;
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

					Data.SetValue(VoxelPosition, FVoxelValue::Full());
					Data.SetMaterial(VoxelPosition, SnowMaterial);
					++ModifiedVoxelCount;
				}
			}
		}
	}

	if (ModifiedVoxelCount > 0)
	{
		Result.AppliedAmount = Request.Amount;
		Result.VoxelWorld = VoxelWorld;
		Result.EditedBounds = CandidateBounds;
		UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, CandidateBounds.Extend(1));
	}

	return Result;
}

bool PaintProcessedTeamSurface(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const int32 TeamId)
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
		MakeTeamPaintMaterial(VoxelWorld->MaterialConfig, TeamId),
		ProcessedVoxels,
		true,
		false,
		true);
	return EditedMaterialBounds.IsValid();
}

void PaintProcessedTeamSurfaceAsync(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	const int32 TeamId,
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
		MakeTeamPaintMaterial(VoxelWorld->MaterialConfig, TeamId),
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
				PaintProcessedTeamSurface(
					VoxelWorld,
					UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
						EditedBounds,
						ModifiedValues,
						true),
					Request.Context.TeamId);
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
				PaintProcessedTeamSurface(
					VoxelWorld,
					MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
					Request.Context.TeamId);
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
		PaintProcessedTeamSurface(
			VoxelWorld,
			MakeNewlyAddedVoxelGroup(ProcessedVoxels, ModifiedValues),
			Request.Context.TeamId);

	}

	Result.VoxelWorld = VoxelWorld;
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

			PaintProcessedTeamSurfaceAsync(
				ValidVoxelWorld,
				PaintVoxels,
				Request.Context.TeamId,
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

bool FDRSnowSurfaceEditor::RepaintSnowMaterialsAtArea(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_Total);
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

	TMap<int32, TArray<FVoxelSurfaceEditsVoxel>> VoxelsByTeam;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_ResolveOwnership);
		TArray<FIntVector> RepaintPositions;
		RepaintPositions.Reserve(ProcessedVoxels.Voxels->Num());
		for (const FVoxelSurfaceEditsVoxel& Voxel : *ProcessedVoxels.Voxels)
		{
			RepaintPositions.Add(Voxel.Position);
		}

		TArray<int32> ResolvedTeamIds;
		TBitArray<> FoundOwnership;
		OwnershipStore.ResolveNearestTeamsAtVoxels(
			VoxelWorld,
			RepaintPositions,
			2,
			ResolvedTeamIds,
			FoundOwnership);

		for (int32 VoxelIndex = 0; VoxelIndex < ProcessedVoxels.Voxels->Num(); ++VoxelIndex)
		{
			const FVoxelSurfaceEditsVoxel& Voxel = (*ProcessedVoxels.Voxels)[VoxelIndex];
			int32 DominantTeamId = ResolvedTeamIds[VoxelIndex];
			// 새로 생긴 눈은 ownership 기록이 더 정확하고, 기존 표면은 Volume 우세 팀으로 fallback 한다.
			if (!FoundOwnership[VoxelIndex])
			{
				const FVector SampleWorldLocation =
					ProcessedVoxels.Info.bHasSurfacePositions
						? VoxelWorld->LocalToGlobalFloat(FVoxelVector(Voxel.SurfacePosition))
						: VoxelWorld->LocalToGlobal(Voxel.Position);
				DominantTeamId = VolumeStore.GetDominantTeamAtLocation(SampleWorldLocation);
			}

			VoxelsByTeam.FindOrAdd(DominantTeamId).Add(Voxel);
		}
	}

	bool bPaintedAny = false;
	for (TPair<int32, TArray<FVoxelSurfaceEditsVoxel>>& TeamVoxels : VoxelsByTeam)
	{
		if (TeamVoxels.Value.Num() == 0)
		{
			continue;
		}

		bPaintedAny |= PaintProcessedTeamSurface(
			VoxelWorld,
			MakeProcessedVoxelGroup(ProcessedVoxels, MoveTemp(TeamVoxels.Value)),
			TeamVoxels.Key);
	}

	return bPaintedAny;
}

bool FDRSnowSurfaceEditor::RepaintSnowMaterialsAtModifiedVoxels(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const FDRSnowOwnershipStore& OwnershipStore,
	const FDRSnowVolumeStore& VolumeStore)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_Dirty_Total);
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

	TArray<FIntVector> DirtyPositions;
	DirtyPositions.Reserve(DirtyVoxels.Voxels->Num());
	for (const FVoxelSurfaceEditsVoxel& Voxel : *DirtyVoxels.Voxels)
	{
		DirtyPositions.Add(Voxel.Position);
	}

	TArray<int32> ResolvedTeamIds;
	TBitArray<> FoundOwnership;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Repaint_Dirty_ResolveOwnership);
		OwnershipStore.ResolveNearestTeamsAtVoxels(
			VoxelWorld,
			DirtyPositions,
			2,
			ResolvedTeamIds,
			FoundOwnership);
	}

	TMap<int32, TArray<FVoxelSurfaceEditsVoxel>> VoxelsByTeam;
	for (int32 VoxelIndex = 0; VoxelIndex < DirtyVoxels.Voxels->Num(); ++VoxelIndex)
	{
		const FVoxelSurfaceEditsVoxel& Voxel = (*DirtyVoxels.Voxels)[VoxelIndex];
		int32 DominantTeamId = ResolvedTeamIds[VoxelIndex];
		if (!FoundOwnership[VoxelIndex])
		{
			DominantTeamId = VolumeStore.GetDominantTeamAtLocation(
				VoxelWorld->LocalToGlobal(Voxel.Position));
		}
		VoxelsByTeam.FindOrAdd(DominantTeamId).Add(Voxel);
	}

	bool bPaintedAny = false;
	for (TPair<int32, TArray<FVoxelSurfaceEditsVoxel>>& TeamVoxels : VoxelsByTeam)
	{
		bPaintedAny |= PaintProcessedTeamSurface(
			VoxelWorld,
			MakeProcessedVoxelGroup(DirtyVoxels, MoveTemp(TeamVoxels.Value)),
			TeamVoxels.Key);
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
