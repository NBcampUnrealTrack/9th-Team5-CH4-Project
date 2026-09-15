#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "VoxelTools/Tools/VoxelToolBase.h"
#include "VoxelTools/VoxelSurfaceEdits.h"
#include "DRDirectionalSurfaceTool.generated.h"

class AVoxelWorld;
class FVoxelData;
// Written by the worker, then read only after its game-thread completion.
// These timings stop at data completion; they do not measure mesh/collision readiness.
struct FDRDirectionalSurfaceEditTimings
{
	double StampMs = 0.0;
	double WorkerQueueMs = 0.0;
	double LockWaitMs = 0.0;
	double DensityMs = 0.0;
	// Post-edit preparation + plugin material kernel, on the SAME density worker.
	double WorkerMaterialMs = 0.0;
	bool bFusedMaterial = false;
	double CallbackMs = 0.0;
	bool bTaskGraphCompletion = false;
	int32 FootprintCount = 0;
	int32 StampCount = 0;
	int64 BoundsCount = 0;
};

using FDRDirectionalSurfaceEditComplete =
	TFunction<void(TArray<FModifiedVoxelValue>&&, FVoxelIntBox)>;

// Runs under the existing write lock for Bounds. No UObject access, render
// updates, nested data locks, or game-thread callbacks are allowed here.
using FDRDirectionalSurfaceWorkerPostEdit =
	TFunction<void(FVoxelData&, const TArray<FModifiedVoxelValue>&, const FVoxelIntBox&)>;

// SurfaceTool의 브러시 모양은 재사용하되, 밀어낼 surface shell을 실제 이동시키지 않고
// 이번 이동이 지나갈 swept volume만 stamp처럼 add/remove 합성하는 Voxel Tool이다.
UCLASS()
class DEEPRAIDERS_API UDRDirectionalSurfaceTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UDRDirectionalSurfaceTool();

	//~ Begin UVoxelToolBase Interface
	virtual void GetToolConfig(FVoxelToolBaseConfig& OutConfig) const override;
	virtual FVoxelIntBoxWithValidity DoEdit() override;
	//~ End UVoxelToolBase Interface

public:
	// true면 눈/땅을 채우고, false면 비운다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite)
	bool bAdd = true;

	// SurfaceTool의 push/pull 세기와 같은 의미다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float Strength = 1.f;

	// 브러시 가장자리 감쇠 비율이다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0", UIMax = "1"))
	float Falloff = 0.35f;

	// SurfaceTool target 값을 voxel density 값으로 줄이는 비율이다.
	UPROPERTY(Category = "Directional Surface Tool", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float DistanceDivisor = 4.f;

public:
	// SurfaceTool과 같은 표면 후보, falloff, strength 결과를 만든다.
	// 이 단계에서는 아직 voxel 값을 수정하지 않는다.
	static FVoxelSurfaceEditsProcessedVoxels FindSurfaceFootprint(
		AVoxelWorld* VoxelWorld,
		const FVector& WorldLocation,
		float Radius,
		float Falloff,
		float Strength,
		bool bAdd);

	// Voxel 표면이 아닌 StaticMesh 등의 Hit 표면을 plane distance field처럼 취급해 footprint를 만든다.
	static FVoxelSurfaceEditsProcessedVoxels MakeVirtualSurfaceFootprint(
		AVoxelWorld* VoxelWorld,
		const FVector& WorldLocation,
		const FVector& SurfaceNormal,
		float Radius,
		float Falloff,
		float Strength,
		bool bAdd,
		int64 VirtualSurfaceSupportMask = 0);

	// Apply all direction-compatible processed samples, including sub-voxel
	// changes that do not cross zero. Preserve the stable v1.2 accumulation rule.
	static float ApplySurfaceVolumeEdit(
		AVoxelWorld* VoxelWorld,
		const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
		float DistanceDivisor,
		bool bAdd,
		TArray<FModifiedVoxelValue>& ModifiedValues,
		FVoxelIntBox& EditedBounds,
		bool bUpdateRender = true);

	// Optional post-edit reuses the plugin material kernel before releasing the
	// density write lock. Completion is still delivered on the game thread.
	static bool ApplySurfaceVolumeEditAsync(
		AVoxelWorld* VoxelWorld,
		const FVoxelSurfaceEditsProcessedVoxels& SurfaceFootprint,
		float DistanceDivisor,
		bool bAdd,
		FDRDirectionalSurfaceEditComplete Completion,
		TSharedPtr<FDRDirectionalSurfaceEditTimings, ESPMode::ThreadSafe> Timings = nullptr,
		FDRDirectionalSurfaceWorkerPostEdit WorkerPostEdit = nullptr);

	// 실제로 값이 변한 voxel 위치만 material paint 입력으로 변환한다.
	static FVoxelSurfaceEditsProcessedVoxels MakeModifiedValueVoxelGroup(
		const FVoxelIntBox& Bounds,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		bool bAdd);

private:
	static float GetModifiedValueAmount(const TArray<FModifiedVoxelValue>& ModifiedValues);
	static float GetSurfaceToolTargetValue(const FVoxelSurfaceEditsVoxel& SurfaceVoxel, float DistanceDivisor);
	static bool IsInsideSweptSurfaceVolume(const FVoxelSurfaceEditsVoxel& SurfaceVoxel, bool bAdd);
};
