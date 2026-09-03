#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "DRSnowSubsystem.generated.h"

class AVoxelWorld;
class FDRSnowAddPipeline;
class FDRSnowMaterialPatchApplyQueue;
class FDRSnowRemovalPipeline;
class FDRSnowRenderUpdateBatcher;
class FDRSnowVoxelContainmentEvaluator;

// Snow 도메인의 유일한 외부 진입점이다.
// 내부 구현의 Volume/Surface/Ownership/Snapshot 모듈 분리는 이 클래스 뒤에 숨긴다.
UCLASS()
class DEEPRAIDERS_API UDRSnowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UDRSnowSubsystem();
	virtual ~UDRSnowSubsystem() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	FDRSnowAddResult AddSnow(
		const FDRSnowSurfaceAddRequest& Request,
		TFunction<void(float)> DirectionalCompletion = {});
	// Multicast 재생은 서버 Sequence를 보존하기 위해 Directional 작업도 동기로 적용한다.
	FDRSnowAddResult ApplyReplicatedSnowAdd(
		const FDRSnowSurfaceAddRequest& Request,
		float AppliedAmount);
	FDRSnowRemoveResult RemoveSnow(
		const FDRSnowSurfaceRemoveRequest& Request,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);
	// 눈총 frustum 전용 제거 경로다.
	FDRSnowRemoveResult RemoveSnowWithAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);
	// Multicast 수신용 제거 경로다. 일반 제거와 달리 서버가 확정한 양을 Volume에 반영한다.
	bool ApplyReplicatedSnowRemoval(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch = nullptr);
	bool ApplyReplicatedSnowAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch = nullptr);
	bool RepaintSnowMaterialsAtArea(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult);

	int32 GetDominantTeamAtLocation(FVector WorldLocation) const;
	FDRSnowControlRatio QuerySnowInBounds(const FBox& WorldBounds) const;

	// 중도 난입 checkpoint 생성/전송에 사용하는 snapshot API다.
	FDRJoinSnapshotSizeReport MeasureCompressedSnapshotSize(AVoxelWorld* TargetVoxelWorld = nullptr, bool bLogResult = true);
	bool CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld = nullptr);
	bool GetLatestCheckpointOperationSequence(int32& OutOperationSequence);
	bool GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint);
	bool GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Snow|Snapshot")
	void ResetCheckpoints();

	/** 새 경기용 눈 데이터와 체크포인트를 모두 비운다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Snow")
	void ResetSnowState();

	bool ApplyCheckpoint(
		FName VoxelWorldName,
		const TArray<uint8>& VoxelSaveData,
		const TArray<uint8>& SnowVolumeData);

private:
	FDRSnowOwnershipStore OwnershipStore;
	FDRSnowVolumeStore VolumeStore;
	FDRSnowSurfaceEditor SurfaceEditor;
	TSharedPtr<FDRSnowVoxelContainmentEvaluator> ContainmentEvaluator;
	TSharedPtr<FDRSnowRenderUpdateBatcher> RenderUpdateBatcher;
	TUniquePtr<FDRSnowSnapshotSerializer> SnapshotSerializer;
	TSharedPtr<FDRSnowRemovalPipeline> RemovalPipeline;
	TSharedPtr<FDRSnowAddPipeline> AddPipeline;
	TSharedPtr<FDRSnowMaterialPatchApplyQueue> MaterialPatchApplyQueue;
	int32 SnowStateGeneration = 0;
};
