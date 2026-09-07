#include "DRDirectionalSurfaceTool.h"

#include "VoxelAsyncWork.h"
#include "VoxelData/VoxelDataImpl.inl"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelWorld.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Async/Async.h"
#include "HAL/IConsoleManager.h"
#include "CoreGlobals.h"

namespace
{
TAutoConsoleVariable<int32> CVarDRSnowFastCompletion(
	TEXT("dr.Snow.FastCompletion"), 1,
	TEXT("1: also offer directional completion to the game-thread task graph. 0: VoxelWorld Tick only."));
TAutoConsoleVariable<int32> CVarDRSnowFastCompletionMax(
	TEXT("dr.Snow.FastCompletionsPerFrame"), 4, TEXT("Maximum additional task-graph completions per process/frame."));
TAutoConsoleVariable<float> CVarDRSnowFastCompletionBudget(
	TEXT("dr.Snow.FastCompletionBudgetMs"), 4.f, TEXT("Soft GT callback/continuation budget per process/frame; original Tick remains the fallback."));

// Process-wide because the task graph belongs to the process. Use separate
// server/client processes for comparisons. Checked only on the game thread.
struct FDRSnowFastCompletionBudget
{
	uint64 Frame = MAX_uint64;
	int32 Count = 0;
	double SpentMs = 0.0;
	bool TryBegin()
	{
		if (Frame != GFrameCounter) { Frame = GFrameCounter; Count = 0; SpentMs = 0.0; }
		if (Count >= FMath::Max(1, CVarDRSnowFastCompletionMax.GetValueOnGameThread()) ||
			SpentMs >= FMath::Max(0.1f, CVarDRSnowFastCompletionBudget.GetValueOnGameThread())) { return false; }
		++Count;
		return true;
	}
};

// Both consumers run on the game thread. Sharing this envelope also avoids
// copying the potentially large ModifiedValues array into TFunction copies.
struct FDRSnowCompletionEnvelope
{
	FDRDirectionalSurfaceEditComplete Completion;
	TArray<FModifiedVoxelValue> ModifiedValues;
	bool bConsumed = false;
};

class FDRDirectionalSurfaceEditWork final : public FVoxelAsyncWork
{
public:
	FDRDirectionalSurfaceEditWork(AVoxelWorld& VoxelWorld, TFunction<void(FVoxelData&)>&& InWork)
		: FVoxelAsyncWork(TEXT("DR Directional Surface Edit"), 1e9, true)
		, Data(VoxelWorld.GetDataSharedPtr())
		, Work(MoveTemp(InWork))
	{
	}

	virtual uint32 GetPriority() const override
	{
		return 0;
	}

	virtual void DoWork() override
	{
		const auto PinnedData = Data.Pin();
		if (PinnedData.IsValid())
		{
			Work(*PinnedData);
		}
	}

private:
	TVoxelWeakPtr<FVoxelData> Data;
	TFunction<void(FVoxelData&)> Work;
};
}

static float GetSmoothFalloffWeight(const float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	return 1.f - ClampedAlpha * ClampedAlpha * (3.f - 2.f * ClampedAlpha);
}

UDRDirectionalSurfaceTool::UDRDirectionalSurfaceTool()
{
	ToolName = TEXT("DR Voxel Directional Surface Tool");
}

void UDRDirectionalSurfaceTool::GetToolConfig(FVoxelToolBaseConfig& OutConfig) const
{
	OutConfig.bHasAlignment = true;
	OutConfig.Alignment = EVoxelToolAlignment::Surface;
}

float UDRDirectionalSurfaceTool::GetModifiedValueAmount(const TArray<FModifiedVoxelValue>& ModifiedValues)
{
	float ModifiedValueAmount = 0.f;
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		ModifiedValueAmount += FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue);
	}

	return ModifiedValueAmount;
}

float UDRDirectionalSurfaceTool::GetSurfaceToolTargetValue(
	const FVoxelSurfaceEditsVoxel& SurfaceVoxel,
	const float DistanceDivisor)
{
	return (SurfaceVoxel.Value + SurfaceVoxel.Strength) / DistanceDivisor;
}

bool UDRDirectionalSurfaceTool::IsInsideSweptSurfaceVolume(
	const FVoxelSurfaceEditsVoxel& SurfaceVoxel,
	const bool bAdd)
{
	const float OldDistance = SurfaceVoxel.Value;
	const float TargetDistance = SurfaceVoxel.Value + SurfaceVoxel.Strength;
	return bAdd
		? OldDistance > 0.f && TargetDistance <= 0.f
		: OldDistance <= 0.f && TargetDistance > 0.f;
}

FVoxelIntBoxWithValidity UDRDirectionalSurfaceTool::DoEdit()
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

FVoxelSurfaceEditsProcessedVoxels UDRDirectionalSurfaceTool::FindSurfaceFootprint(
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

FVoxelSurfaceEditsProcessedVoxels UDRDirectionalSurfaceTool::MakeVirtualSurfaceFootprint(
	AVoxelWorld* VoxelWorld,
	const FVector& WorldLocation,
	const FVector& SurfaceNormal,
	float Radius,
	float Falloff,
	float Strength,
	bool bAdd)
{
	FVoxelSurfaceEditsProcessedVoxels Result;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Radius <= 0.f || Strength <= 0.f)
	{
		return Result;
	}

	const FVector SafeNormal = SurfaceNormal.GetSafeNormal();
	if (SafeNormal.IsNearlyZero())
	{
		return Result;
	}

	const float ShellWorldThickness = FMath::Max(VoxelWorld->VoxelSize, VoxelWorld->VoxelSize * Strength);
	const FVoxelIntBox Bounds =
		UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
			VoxelWorld,
			WorldLocation,
			Radius + ShellWorldThickness);
	if (!Bounds.IsValid())
	{
		return Result;
	}

	TArray<FVoxelSurfaceEditsVoxel> Voxels;
	for (int32 Z = Bounds.Min.Z; Z < Bounds.Max.Z; ++Z)
	{
		for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
		{
			for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
			{
				const FIntVector Position(X, Y, Z);
				const FVector VoxelWorldLocation = VoxelWorld->LocalToGlobal(Position);
				const FVector Delta = VoxelWorldLocation - WorldLocation;
				const float SignedWorldDistance = FVector::DotProduct(Delta, SafeNormal);
				const float SignedVoxelDistance = SignedWorldDistance / FMath::Max(KINDA_SMALL_NUMBER, VoxelWorld->VoxelSize);

				const FVector PlanarDelta = Delta - SafeNormal * SignedWorldDistance;
				const float PlanarDistance = PlanarDelta.Size();
				if (PlanarDistance > Radius)
				{
					continue;
				}

				const float FalloffStart = Radius * FMath::Clamp(Falloff, 0.f, 1.f);
				float FalloffWeight = 1.f;
				if (Radius > FalloffStart && PlanarDistance > FalloffStart)
				{
					FalloffWeight = GetSmoothFalloffWeight((PlanarDistance - FalloffStart) / (Radius - FalloffStart));
				}
				if (FalloffWeight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				// A constant one-voxel backing creates a flat cylindrical side wall.
				// Taper the virtual support with the same radial falloff as the snow
				// footprint so its thickness collapses toward the patch boundary.
				// This only clips virtual Add candidates; density math stays unchanged.
				const float BackingDepth = VoxelWorld->VoxelSize * FalloffWeight;
				if (bAdd && SignedWorldDistance < -BackingDepth)
				{
					continue;
				}

				FVoxelSurfaceEditsVoxel Voxel;
				Voxel.Position = Position;
				Voxel.Normal = SafeNormal;
				Voxel.Value = SignedVoxelDistance;
				Voxel.SurfacePosition = VoxelWorld->GlobalToLocalFloat(WorldLocation).ToFloat();
				Voxel.Strength = Strength * FalloffWeight * (bAdd ? -1.f : 1.f);
				Voxels.Add(Voxel);
			}
		}
	}

	if (Voxels.Num() == 0)
	{
		return Result;
	}

	Result.Bounds = Bounds;
	Result.Info.bHasValues = true;
	Result.Info.bHasNormals = true;
	Result.Info.bHasSurfacePositions = true;
	Result.Info.bHasExactDistanceField = true;
	Result.Voxels = MakeVoxelShared<TArray<FVoxelSurfaceEditsVoxel>>(MoveTemp(Voxels));
	return Result;
}

float UDRDirectionalSurfaceTool::ApplySurfaceVolumeEdit(
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

	TMap<FIntVector, float> StampValueByPosition;
	for (const FVoxelSurfaceEditsVoxel& SurfaceVoxel : *SurfaceFootprint.Voxels)
	{
		const float SignedStrength = SurfaceVoxel.Strength;
		// SurfaceTool은 target 값을 그대로 쓰지만, 이 툴은 요청 방향과 반대인 후보를 버린다.
		if ((bAdd && SignedStrength >= 0.f) || (!bAdd && SignedStrength <= 0.f))
		{
			continue;
		}
		// EditVoxelValues (Legacy) applies the processed distance-field value to
		// every surface sample, not only to samples crossing zero this frame.
		// Keeping only zero-crossing samples makes repeated sub-voxel deposits
		// sparse and prevents a stable second layer from forming.

		const float TargetValue = GetSurfaceToolTargetValue(SurfaceVoxel, DistanceDivisor);
		float& StoredValue = StampValueByPosition.FindOrAdd(SurfaceVoxel.Position, TargetValue);
		StoredValue = bAdd ? FMath::Min(StoredValue, TargetValue) : FMath::Max(StoredValue, TargetValue);
	}

	if (StampValueByPosition.Num() == 0)
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
			const float* StampValue = StampValueByPosition.Find(FIntVector(X, Y, Z));
			if (!StampValue)
			{
				return;
			}

			const float CurrentValue = Value.ToFloat();
			// FVoxelValue는 값이 낮을수록 filled, 높을수록 empty 쪽이다.
			// 그래서 Add는 swept volume 중 비어 있던 곳만 채우고, Remove는 그 stamp 부피만 비운다.
			if ((bAdd && *StampValue < CurrentValue) || (!bAdd && *StampValue > CurrentValue))
			{
				Value = FVoxelValue(*StampValue);
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

bool UDRDirectionalSurfaceTool::ApplySurfaceVolumeEditAsync(
	AVoxelWorld* VoxelWorld,
	const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
	float DistanceDivisor,
	bool bAdd,
	FDRDirectionalSurfaceEditComplete Completion,
	TSharedPtr<FDRDirectionalSurfaceEditTimings, ESPMode::ThreadSafe> Timings,
	FDRDirectionalSurfaceWorkerPostEdit WorkerPostEdit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Directional_BuildStamp);
	const double StampStart = Timings ? FPlatformTime::Seconds() : 0.0;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || DistanceDivisor <= 0.f ||
		SurfaceFootprint.Voxels->Num() == 0 || !Completion)
	{
		return false;
	}

	const FVoxelIntBox Bounds = SurfaceFootprint.Bounds;
	if (!Bounds.IsValid())
	{
		return false;
	}

	TMap<FIntVector, float> StampValueByPosition;
	for (const FVoxelSurfaceEditsVoxel& SurfaceVoxel : *SurfaceFootprint.Voxels)
	{
		const float SignedStrength = SurfaceVoxel.Strength;
		if ((bAdd && SignedStrength >= 0.f) || (!bAdd && SignedStrength <= 0.f))
		{
			continue;
		}

		const float TargetValue = GetSurfaceToolTargetValue(SurfaceVoxel, DistanceDivisor);
		float& StoredValue = StampValueByPosition.FindOrAdd(SurfaceVoxel.Position, TargetValue);
		StoredValue = bAdd ? FMath::Min(StoredValue, TargetValue) : FMath::Max(StoredValue, TargetValue);
	}

	if (StampValueByPosition.IsEmpty())
	{
		return false;
	}

	const auto GameThreadTasks = VoxelWorld->GetGameThreadTasks();
	const TWeakObjectPtr<AVoxelWorld> WeakVoxelWorld(VoxelWorld);
	const bool bFastCompletion = CVarDRSnowFastCompletion.GetValueOnGameThread() != 0;
	if (Timings)
	{
		Timings->StampMs = (FPlatformTime::Seconds() - StampStart) * 1000.0;
		Timings->bFusedMaterial = !!WorkerPostEdit;
		Timings->FootprintCount = SurfaceFootprint.Voxels->Num();
		Timings->StampCount = StampValueByPosition.Num();
		Timings->BoundsCount =
			(static_cast<int64>(Bounds.Max.X) - Bounds.Min.X) *
			(static_cast<int64>(Bounds.Max.Y) - Bounds.Min.Y) *
			(static_cast<int64>(Bounds.Max.Z) - Bounds.Min.Z);
	}
	const double DispatchTime = Timings ? FPlatformTime::Seconds() : 0.0;
	auto Work = [
		Bounds,
		bAdd,
		Timings,
		DispatchTime,
		WeakVoxelWorld,
		bFastCompletion,
		WorkerPostEdit = MoveTemp(WorkerPostEdit),
		StampValueByPosition = MoveTemp(StampValueByPosition),
		GameThreadTasks,
		Completion = MoveTemp(Completion)](FVoxelData& Data) mutable
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Directional_Worker);
		const double WorkerStart = Timings ? FPlatformTime::Seconds() : 0.0;
		if (Timings)
		{
			Timings->WorkerQueueMs = (WorkerStart - DispatchTime) * 1000.0;
		}
		TVoxelDataImpl<FModifiedVoxelValue> DataImpl(Data, false, true);
		{
			const double LockStart = Timings ? FPlatformTime::Seconds() : 0.0;
			FVoxelWriteScopeLock Lock(Data, Bounds, FUNCTION_FNAME);
			const double DensityStart = Timings ? FPlatformTime::Seconds() : 0.0;
			if (Timings)
			{
				Timings->LockWaitMs = (DensityStart - LockStart) * 1000.0;
			}
			DataImpl.Set<FVoxelValue>(Bounds, [&](int32 X, int32 Y, int32 Z, FVoxelValue& Value)
			{
				const float* StampValue = StampValueByPosition.Find(FIntVector(X, Y, Z));
				if (!StampValue)
				{
					return;
				}

				const float CurrentValue = Value.ToFloat();
				if ((bAdd && *StampValue < CurrentValue) || (!bAdd && *StampValue > CurrentValue))
				{
					Value = FVoxelValue(*StampValue);
				}
			});
			const double DensityDone = Timings ? FPlatformTime::Seconds() : 0.0;
			if (Timings)
			{
				Timings->DensityMs = (DensityDone - DensityStart) * 1000.0;
			}
			if (WorkerPostEdit && !DataImpl.ModifiedValues.IsEmpty())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Fused_PluginMaterial);
				const double MaterialStart = Timings ? FPlatformTime::Seconds() : 0.0;
				WorkerPostEdit(Data, DataImpl.ModifiedValues, Bounds);
				if (Timings)
				{
					Timings->WorkerMaterialMs = (FPlatformTime::Seconds() - MaterialStart) * 1000.0;
				}
			}
		}

		const double WorkerDone = Timings ? FPlatformTime::Seconds() : 0.0;
		auto Envelope = MakeShared<FDRSnowCompletionEnvelope, ESPMode::ThreadSafe>();
		Envelope->Completion = MoveTemp(Completion);
		Envelope->ModifiedValues = MoveTemp(DataImpl.ModifiedValues);
		auto RunOnce = [
			Bounds,
			Timings,
			WorkerDone,
			Envelope](const bool bFromTaskGraph)
		{
			check(IsInGameThread());
			if (Envelope->bConsumed)
			{
				return;
			}
			static FDRSnowFastCompletionBudget FastBudget;
			if (bFromTaskGraph && !FastBudget.TryBegin())
			{
				return; // Still owned by the original VoxelWorld queue.
			}
			Envelope->bConsumed = true;
			const double CallbackStart = bFromTaskGraph ? FPlatformTime::Seconds() : 0.0;
			auto Callback = MoveTemp(Envelope->Completion);
			if (Timings)
			{
				Timings->CallbackMs = (FPlatformTime::Seconds() - WorkerDone) * 1000.0;
				Timings->bTaskGraphCompletion = bFromTaskGraph;
			}
			Callback(MoveTemp(Envelope->ModifiedValues), Bounds);
			if (bFromTaskGraph)
			{
				FastBudget.SpentMs += (FPlatformTime::Seconds() - CallbackStart) * 1000.0;
			}
		};
		// Keep the plugin queue's existing teardown/flush behavior. Whichever
		// game-thread consumer gets here first consumes the result exactly once.
		GameThreadTasks->AddTask([RunOnce]() { RunOnce(false); });
		if (bFastCompletion)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakVoxelWorld, GameThreadTasks, RunOnce]()
			{
				AVoxelWorld* CurrentWorld = WeakVoxelWorld.Get();
				if (IsValid(CurrentWorld) && CurrentWorld->IsCreated() &&
					CurrentWorld->GetGameThreadTasks() == GameThreadTasks)
				{
					RunOnce(true);
				}
			});
		}
	};

	FVoxelToolHelpers::StartAsyncEditTask(
		VoxelWorld,
		new FDRDirectionalSurfaceEditWork(*VoxelWorld, MoveTemp(Work)));
	return true;
}

FVoxelSurfaceEditsProcessedVoxels UDRDirectionalSurfaceTool::MakeModifiedValueVoxelGroup(
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
