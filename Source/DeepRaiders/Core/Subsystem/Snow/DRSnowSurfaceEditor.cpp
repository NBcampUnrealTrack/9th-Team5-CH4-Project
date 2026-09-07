#include "DRSnowSurfaceEditor.h"

#include "DeepRaiders/Snow/DRDirectionalSurfaceTool.h"
#include "DeepRaiders/Snow/DRSnowAbsorbTool.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelTools/Gen/VoxelBoxTools.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/Gen/VoxelSurfaceEditTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelPaintMaterial.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelData/VoxelDataImpl.inl"
#include "VoxelTools/Impl/VoxelSurfaceEditToolsImpl.h"
#include "VoxelTools/Impl/VoxelSurfaceEditToolsImpl.inl"
#include "VoxelMaterial.h"
#include "VoxelWorld.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

namespace
{
constexpr float SnowSurfaceDistanceDivisor = 4.f;
constexpr float SnowSurfaceFalloff = 0.35f;

TAutoConsoleVariable<int32> CVarDRSnowPatchVersion(
	TEXT("dr.Snow.PatchVersion"), 105,
	TEXT("Compiled SnowEdit project patch version: 105 = 1.5; plugin stays 104."), ECVF_ReadOnly);

TAutoConsoleVariable<int32> CVarDRSnowFusedMaterialEdit(
	TEXT("dr.Snow.FusedMaterialEdit"), 1,
	TEXT("1: run the unchanged plugin material kernel after density on the same worker. 0: v1.2 two-worker path. Requires DirectionalCombinedEdit=1. Restart the world for comparisons."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarDRSnowDirectionalCombinedEdit(
	TEXT("dr.Snow.DirectionalCombinedEdit"),
	1,
	TEXT("1: keep the plugin material pass but skip the duplicate final repaint/update. 0: exact legacy finalization. Restart the test world when comparing."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarDRSnowPerf(
	TEXT("dr.Snow.Perf"),
	0,
	TEXT("1: log Directional pipeline timings and client replay queue events. Does not measure mesh/collision completion."),
	ECVF_Default);

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
		PaintMaterial.MultiIndex.Channel.Channel = MaterialIndex;
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



}

void FDRSnowSurfaceEditor::SetWorld(UWorld* InWorld)
{
	if (World == InWorld)
	{
		return;
	}
	World = InWorld;
	if (IsValid(World))
	{
		const IConsoleVariable* PluginVersion = IConsoleManager::Get().FindConsoleVariable(TEXT("voxel.DR.SnowPatchVersion"));
		const IConsoleVariable* CpuParallel = IConsoleManager::Get().FindConsoleVariable(TEXT("voxel.DR.CpuJumpFloodParallel"));
		const IConsoleVariable* FastCompletion = IConsoleManager::Get().FindConsoleVariable(TEXT("dr.Snow.FastCompletion"));
		const IConsoleVariable* FastReplay = IConsoleManager::Get().FindConsoleVariable(TEXT("dr.Snow.FastReplay"));
		UE_LOG(LogTemp, Log, TEXT("[DRSnowBuild] Version=1.5 PID=%u Role=%s World=%s NetMode=%d CanRender=%d PluginVersion=%d FusedRequested=%d Combined=%d CpuParallel=%d FastCompletion=%d FastReplay=%d"),
			FPlatformProcess::GetCurrentProcessId(),
			World->GetNetMode() == NM_Client ? TEXT("Client") : (World->GetNetMode() == NM_Standalone ? TEXT("Standalone") : TEXT("Server")),
			*World->GetName(), static_cast<int32>(World->GetNetMode()), FApp::CanEverRender() ? 1 : 0,
			PluginVersion ? PluginVersion->GetInt() : -1,
			CVarDRSnowFusedMaterialEdit.GetValueOnGameThread(), CVarDRSnowDirectionalCombinedEdit.GetValueOnGameThread(),
			CpuParallel ? CpuParallel->GetInt() : -1,
			FastCompletion ? FastCompletion->GetInt() : -1, FastReplay ? FastReplay->GetInt() : -1);
		if (!PluginVersion || PluginVersion->GetInt() != 104)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DRSnowBuild] Plugin version mismatch. v1.5 requires the committed v1.4 plugin (104)."));
		}
	}
}

bool FDRSnowSurfaceEditor::IsCombinedDirectionalEditEnabled()
{
	return CVarDRSnowDirectionalCombinedEdit.GetValueOnGameThread() != 0;
}

bool FDRSnowSurfaceEditor::IsDirectionalPerfLoggingEnabled()
{
	return CVarDRSnowPerf.GetValueOnGameThread() != 0;
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

void FDRSnowSurfaceEditor::FillAddedSnowMaterials(
	const FDRSnowSurfaceEditResult& EditResult,
	const int32 TeamId)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_FillAddedMaterials);
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (EditResult.AppliedAmount <= 0.f || !EditResult.EditedBounds.IsValid() ||
		!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		VoxelWorld->MaterialConfig == EVoxelMaterialConfig::RGB)
	{
		return;
	}

	const FVoxelPaintMaterial PaintMaterial = MakeIndexPaintMaterial(
		VoxelWorld->MaterialConfig, DRSnowMaterialMapping::TeamToMaterialIndex(TeamId));
	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelWriteScopeLock Lock(Data, EditResult.EditedBounds, FUNCTION_FNAME);
		// 표면 목록이 아닌 실제 값 변경 목록을 사용해 새로 추가한 내부까지 칠한다.
		// Bounds는 잠금 범위일 뿐이다. 기존 고체와 도구 밖의 복셀은 보존한다.
		for (const FModifiedVoxelValue& Value : EditResult.ModifiedValues)
		{
			if (Value.OldValue <= 0.f || Value.NewValue >= Value.OldValue)
			{
				continue;
			}
			FVoxelMaterial Material = Data.GetMaterial(Value.Position, 0);
			PaintMaterial.ApplyToMaterial(Material, 1.f);
			Data.SetMaterial(Value.Position, Material);
		}
	}
	UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, EditResult.EditedBounds.Extend(1));
}

bool FDRSnowSurfaceEditor::AddDirectionalSnowAtAreaAsync(
	const FDRSnowSurfaceAddRequest& Request,
	TFunction<void(FDRSnowSurfaceEditResult&&)> Completion)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Directional_Prepare);
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

	const bool bCombined = IsCombinedDirectionalEditEnabled();
	const bool bMeasure = IsDirectionalPerfLoggingEnabled();
	TSharedPtr<FDRDirectionalSurfaceEditTimings, ESPMode::ThreadSafe> Timings;
	if (bMeasure)
	{
		Timings = MakeShared<FDRDirectionalSurfaceEditTimings, ESPMode::ThreadSafe>();
	}
	const double FootprintStart = bMeasure ? FPlatformTime::Seconds() : 0.0;
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
			true,
			Request.VirtualSurfaceSupportMask);
	}

	const double FootprintMs = bMeasure ? (FPlatformTime::Seconds() - FootprintStart) * 1000.0 : 0.0;
	const TWeakObjectPtr<AVoxelWorld> WeakVoxelWorld = VoxelWorld;
	const bool bFusedMaterial = bCombined && CVarDRSnowFusedMaterialEdit.GetValueOnGameThread() != 0 &&
		VoxelWorld->MaterialConfig != EVoxelMaterialConfig::RGB;
	FDRDirectionalSurfaceWorkerPostEdit WorkerPostEdit;
	if (bFusedMaterial)
	{
		// Snapshot configuration on the game thread. The worker touches only
		// FVoxelData and value types; it never dereferences VoxelWorld/UObjects.
		const FVoxelPaintMaterial PaintMaterial = MakeIndexPaintMaterial(
			VoxelWorld->MaterialConfig, DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId));
		WorkerPostEdit = [PaintMaterial](FVoxelData& Data,
			const TArray<FModifiedVoxelValue>& ModifiedValues, const FVoxelIntBox& Bounds)
		{
			const FVoxelSurfaceEditsProcessedVoxels PaintVoxels =
				UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(Bounds, ModifiedValues, true);
			if (!FVoxelSurfaceEditToolsImpl::ShouldCompute(PaintVoxels))
			{
				return;
			}
			// Same adapter flags and kernel as EditVoxelMaterialsAsync(true, false, false).
			// The enclosing density worker already holds the write lock for Bounds.
			auto MaterialData = TVoxelDataImpl<FModifiedVoxelMaterial>(Data, true, false);
			FVoxelSurfaceEditToolsImpl::EditVoxelMaterials(MaterialData,
				FVoxelSurfaceEditToolsImpl::GetBounds(PaintVoxels), PaintMaterial, PaintVoxels);
		};
	}
	return UDRDirectionalSurfaceTool::ApplySurfaceVolumeEditAsync(
		VoxelWorld,
		SurfaceFootprint,
		SnowSurfaceDistanceDivisor,
		true,
		[WeakVoxelWorld, Request, bCombined, bFusedMaterial, Timings, FootprintMs, Completion = MoveTemp(Completion)](
			TArray<FModifiedVoxelValue>&& ModifiedValues,
			FVoxelIntBox EditedBounds) mutable
		{
			FDRSnowSurfaceEditResult Result;
			Result.bAddedMaterialsFinalized = false;
			Result.FootprintMs = FootprintMs;
			Result.DirectionalTimings = Timings;
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

			Result.VoxelWorld = ValidVoxelWorld;
			Result.EditedBounds = EditedBounds;
			Result.ModifiedValues = MoveTemp(ModifiedValues);
			Result.bUseModifiedValuesForVolume = true;
			if (bFusedMaterial)
			{
				Result.bAddedMaterialsFinalized = true;
				UVoxelBlueprintLibrary::UpdateBounds(ValidVoxelWorld, EditedBounds.Extend(1));
				Completion(MoveTemp(Result));
				return;
			}

			// Always use the plugin's established material edit. Combined mode only
			// removes the later duplicate direct repaint and second UpdateBounds.
			const FVoxelSurfaceEditsProcessedVoxels PaintVoxels =
				UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
					EditedBounds,
					Result.ModifiedValues,
					true);
			const double LegacyMaterialStart = Timings ? FPlatformTime::Seconds() : 0.0;

			PaintProcessedMaterialSurfaceAsync(
				ValidVoxelWorld,
				PaintVoxels,
				DRSnowMaterialMapping::TeamToMaterialIndex(Request.Context.TeamId),
				[Result = MoveTemp(Result), bCombined, LegacyMaterialStart, Completion = MoveTemp(Completion)]() mutable
				{
					if (Result.DirectionalTimings)
					{
						Result.LegacyMaterialMs = (FPlatformTime::Seconds() - LegacyMaterialStart) * 1000.0;
					}
					Result.bAddedMaterialsFinalized = bCombined;
					if (AVoxelWorld* PaintedVoxelWorld = Result.VoxelWorld.Get();
						IsValid(PaintedVoxelWorld) && PaintedVoxelWorld->IsCreated() &&
						Result.EditedBounds.IsValid())
					{
						// Legacy A/B path: pipeline FillAddedSnowMaterials requests another update.
						UVoxelBlueprintLibrary::UpdateBounds(
							PaintedVoxelWorld,
							Result.EditedBounds.Extend(1));
					}
					Completion(MoveTemp(Result));
				});
		},
		Timings,
		MoveTemp(WorkerPostEdit));
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
