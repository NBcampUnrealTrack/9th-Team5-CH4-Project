#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

class AVoxelWorld;
class FDRSnowOwnershipStore;
class FDRSnowRenderUpdateBatcher;
class FDRSnowSurfaceEditor;
class FDRSnowVolumeStore;
class UWorld;

// 눈 추가의 동기 경로와 Directional 비동기 큐를 함께 소유한다.
// 비동기 완료 이후 Ownership/Volume/Render 갱신 순서도 이 클래스 안에서 보장한다.
class DEEPRAIDERS_API FDRSnowAddPipeline : public TSharedFromThis<FDRSnowAddPipeline>
{
public:
	FDRSnowAddPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowOwnershipStore& InOwnershipStore,
		FDRSnowVolumeStore& InVolumeStore,
		FDRSnowRenderUpdateBatcher& InRenderUpdateBatcher);

	void Initialize(UWorld* InWorld, int32 InitialStateGeneration);
	FDRSnowAddResult Execute(
		const FDRSnowSurfaceAddRequest& Request,
		TFunction<void(float)> DirectionalCompletion = {});
	FDRSnowAddResult Replay(
		const FDRSnowSurfaceAddRequest& Request,
		float AuthoritativeAmount);
	void Reset(int32 NewStateGeneration);

private:
	struct FPendingDirectionalAdd
	{
		FDRSnowSurfaceAddRequest Request;
		TFunction<void(float)> Completion;
		int32 StateGeneration = 0;
	};

	void ProcessNextDirectionalAdd();
	void HandleDirectionalAddCompleted(
		const FDRSnowSurfaceAddRequest& Request,
		int32 RequestGeneration,
		TFunction<void(float)> Completion,
		FDRSnowSurfaceEditResult&& EditResult);
	void ApplyAddedSurfaceEdit(
		const FDRSnowSurfaceAddRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		float VolumeAmount = -1.f);
	void AddVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceAddRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxAddedAmount);

	TWeakObjectPtr<UWorld> World;
	FDRSnowSurfaceEditor& SurfaceEditor;
	FDRSnowOwnershipStore& OwnershipStore;
	FDRSnowVolumeStore& VolumeStore;
	FDRSnowRenderUpdateBatcher& RenderUpdateBatcher;
	TQueue<FPendingDirectionalAdd> PendingDirectionalAdds;
	int32 CurrentStateGeneration = 0;
	bool bDirectionalAddInProgress = false;
};
