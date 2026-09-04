#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowAddPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowRemovalPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVoxelContainmentEvaluator.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "DRSnowSubsystem.generated.h"

class AVoxelWorld;
class FDRSnowMaterialPatchApplyQueue;

// Snow 도메인의 유일한 외부 진입점이다.
// 내부 구현의 Volume/Surface/Ownership/Snapshot 모듈 분리는 이 클래스 뒤에 숨긴다.
UCLASS()
class DEEPRAIDERS_API UDRSnowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UDRSnowSubsystem();
	virtual ~UDRSnowSubsystem() override;

	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	FDRSnowAddResult AddSnow(const FDRSnowSurfaceAddRequest& Request);
	// Multicast 재생은 서버 Sequence를 보존하며 서버가 확정한 양을 Volume에 반영한다.
	FDRSnowAddResult ApplyReplicatedSnowAdd(
		const FDRSnowSurfaceAddRequest& Request,
		float AppliedAmount);
	FDRSnowRemoveResult RemoveSnow(
		const FDRSnowSurfaceRemoveRequest& Request,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);
	// 클라이언트 표현만 즉시 편집한다. Volume과 Material은 서버 확정 수신 시 반영한다.
	FDRSnowRemoveResult PredictSnowRemoval(
		const FDRSnowSurfaceRemoveRequest& Request);
	// 눈총 frustum 전용 제거 경로다.
	FDRSnowRemoveResult RemoveSnowWithAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);
	FDRSnowRemoveResult PredictSnowAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request);
	// Multicast 수신용 제거 경로다. 일반 제거와 달리 서버가 확정한 양을 Volume에 반영한다.
	bool ApplyReplicatedSnowRemoval(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch = nullptr);
	bool ApplyReplicatedSnowAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch = nullptr);

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
	struct FPendingRemovalPrediction
	{
		FDRSnowPredictionKey PredictionKey;
		FDRSnowSurfaceEditResult SurfaceEdit;
		EDRSnowRemovalPath RemovalPath = EDRSnowRemovalPath::Standard;
	};

	FDRSnowRemoveResult PredictSnowRemovalInternal(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath);
	bool ConsumeMatchingRemovalPrediction(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath,
		FPendingRemovalPrediction& OutPrediction);
	void ConfirmPredictedRemoval(
		const FPendingRemovalPrediction& Prediction,
		const FDRSnowSurfaceRemoveRequest& AuthoritativeRequest,
		float AuthoritativeAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch);
	void ResetRemovalPredictions();

	FDRSnowOwnershipStore OwnershipStore;
	FDRSnowVolumeStore VolumeStore;
	FDRSnowSurfaceEditor SurfaceEditor;
	TUniquePtr<FDRSnowVoxelContainmentEvaluator> ContainmentEvaluator;
	TUniquePtr<FDRSnowSnapshotSerializer> SnapshotSerializer;
	TUniquePtr<FDRSnowRemovalPipeline> RemovalPipeline;
	TUniquePtr<FDRSnowAddPipeline> AddPipeline;
	TSharedPtr<FDRSnowMaterialPatchApplyQueue> MaterialPatchApplyQueue;
	TArray<FPendingRemovalPrediction> PendingRemovalPredictions;
};
