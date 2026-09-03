#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "VoxelIntBox.h"

class AVoxelWorld;
class UWorld;

// 짧은 시간 안에 들어온 Voxel 렌더 갱신을 World별로 모으고,
// 서로 겹치는 Bounds만 병합해서 한 번에 적용한다.
class DEEPRAIDERS_API FDRSnowRenderUpdateBatcher
{
public:
	~FDRSnowRenderUpdateBatcher();

	void Initialize(UWorld* InWorld);
	void Enqueue(AVoxelWorld* VoxelWorld, const FVoxelIntBox& Bounds);
	void Reset();

private:
	struct FPendingUpdate
	{
		TWeakObjectPtr<AVoxelWorld> VoxelWorld;
		TArray<FVoxelIntBox> Bounds;
	};

	void Flush();

	TWeakObjectPtr<UWorld> World;
	TArray<FPendingUpdate> PendingUpdates;
	FTimerHandle FlushTimerHandle;
};
