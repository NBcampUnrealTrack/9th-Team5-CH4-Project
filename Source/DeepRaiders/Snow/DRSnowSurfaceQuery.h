#pragma once

#include "CoreMinimal.h"
#include "VoxelTools/VoxelSurfaceTools.h"

class AVoxelWorld;

namespace DRSnowSurfaceQuery
{
	// Synchronous result, without a game-thread render/GPU fence.
	void FindSurface(FVoxelSurfaceEditsVoxels& OutVoxels, AVoxelWorld* World, const FVoxelIntBox& Bounds);
}
