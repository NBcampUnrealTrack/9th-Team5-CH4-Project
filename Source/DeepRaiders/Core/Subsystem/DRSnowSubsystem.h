#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "DRSnowSubsystem.generated.h"

class AVoxelWorld;

struct FDRSnowPendingRenderUpdate
{
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;
	TArray<FVoxelIntBox> Bounds;
};

// Snow 도메인의 유일한 외부 진입점이다.
// 내부 구현의 Volume/Surface/Ownership/Snapshot 모듈 분리는 이 클래스 뒤에 숨긴다.
UCLASS()
class DEEPRAIDERS_API UDRSnowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UDRSnowSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	FDRSnowAddResult AddSnow(const FDRSnowSurfaceAddRequest& Request);
	FDRSnowRemoveResult RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request);
	// Multicast 수신용 제거 경로다. 일반 제거와 달리 서버가 확정한 양을 Volume에 반영한다.
	bool ApplyReplicatedSnowRemoval(const FDRSnowSurfaceRemoveRequest& Request, float AppliedAmount);
	bool RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	int32 GetDominantTeamAtLocation(FVector WorldLocation) const;
	FDRSnowControlRatio QuerySnowInBounds(const FBox& WorldBounds) const;

	// 중도 난입 checkpoint 생성/전송에 사용하는 snapshot API다.
	FDRJoinSnapshotSizeReport MeasureCompressedSnapshotSize(AVoxelWorld* TargetVoxelWorld = nullptr, bool bLogResult = true);
	bool CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld = nullptr);
	bool GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint);
	bool GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Snow|Snapshot")
	void ResetCheckpoints();

	/** 새 경기용 눈 데이터와 체크포인트를 모두 비운다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Snow")
	void ResetSnowState();

	bool ApplyCheckpoint(FName VoxelWorldName, const TArray<uint8>& VoxelSaveData, const TArray<uint8>& SnowVolumeData, const TArray<uint8>& OwnershipData);

private:
	// 제거 brush는 실제로 변경된 voxel만 반환한다.
	// 이 결과를 기준으로 해야 Volume 원본 데이터가 Voxel 표현과 같은 변화만 기록한다.
	void ApplyAddedSurfaceEdit(
		const FDRSnowSurfaceAddRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult);
	void ApplyRemovedSurfaceEdit(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		float VolumeAmount);
	void AddVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceAddRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxAddedAmount);
	void RemoveVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceRemoveRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxRemovedAmount);
	void ProcessNextDirectionalAdd();
	void QueueRenderUpdate(AVoxelWorld* VoxelWorld, const FVoxelIntBox& Bounds);
	void FlushRenderUpdates();

	FDRSnowOwnershipStore OwnershipStore;
	FDRSnowVolumeStore VolumeStore;
	FDRSnowSurfaceEditor SurfaceEditor;
	TUniquePtr<FDRSnowSnapshotSerializer> SnapshotSerializer;
	TQueue<FDRSnowSurfaceAddRequest> DirectionalAddQueue;
	bool bDirectionalAddInProgress = false;
	TArray<FDRSnowPendingRenderUpdate> PendingRenderUpdates;
	FTimerHandle RenderUpdateTimerHandle;
};
