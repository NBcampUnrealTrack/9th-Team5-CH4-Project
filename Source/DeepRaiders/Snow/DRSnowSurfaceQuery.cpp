#include "DRSnowSurfaceQuery.h"
#include "Async/Async.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelWorld.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSnowSurfaceQueryCPU(TEXT("dr.Snow.SurfaceQueryCPU"), 1,
		TEXT("1: CPU surface query on a worker, avoiding render/GPU fence waits. 0: legacy plugin query for A/B profiling."));
}

void DRSnowSurfaceQuery::FindSurface(FVoxelSurfaceEditsVoxels& OutVoxels,
	AVoxelWorld* World, const FVoxelIntBox& Bounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_SurfaceQuery);
	check(IsInGameThread());
	OutVoxels = FVoxelSurfaceEditsVoxels();
	if (!IsValid(World) || !World->IsCreated() || !Bounds.IsValid()) { return; }
	if (CVarSnowSurfaceQueryCPU.GetValueOnGameThread() == 0)
	{
		UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(OutVoxels, World, Bounds, true);
		return;
	}
	// Capture data ownership on the game thread; never dereference UObjects on the worker.
	const auto Data = World->GetDataSharedPtr();
	OutVoxels = Async(EAsyncExecution::ThreadPool, [Data, Bounds]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_SurfaceQuery_CPU);
		FVoxelReadScopeLock Lock(*Data, Bounds.Extend(1), FUNCTION_FNAME);
		// The unchanged plugin selects CPU when invoked off the game thread.
		return UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceFieldImpl(*Data, Bounds, true);
	}).Get();
	// Preserve the existing immediate result contract used by removal and its network replay.
}
