#pragma once

#include "CoreMinimal.h"
#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

class AVoxelWorld;
class FDRSnowAddPipeline;
class FDRSnowMaterialPatchApplyQueue;
class FDRSnowOwnershipStore;
class FDRSnowSurfaceEditor;
class FDRSnowVolumeStore;
class UWorld;

// 눈 파내기 방식 구분
enum class EDRSnowRemovalPath : uint8
{
	Standard, // 일반 파내기 (원형/구형 범위)
	Absorb    // 눈총 흡수 파내기 (카메라 시야각 Frustum 범위)
};

// 서버가 확정한 파내기 결과를 클라이언트 예측 조정 단계에 전달하는 매개변수 객체
struct FDRSnowServerResponse
{
	FDRSnowServerResponse(
		const float InAuthoritativeAmount,
		const FDRSnowMaterialPatch* InMaterialPatch,
		const EDRSnowRemovalPath InRemovalPath)
		: AuthoritativeAmount(InAuthoritativeAmount)
		, MaterialPatch(InMaterialPatch)
		, RemovalPath(InRemovalPath)
	{
	}

	float AuthoritativeAmount = 0.f;
	const FDRSnowMaterialPatch* MaterialPatch = nullptr;
	EDRSnowRemovalPath RemovalPath = EDRSnowRemovalPath::Standard;
};

// 서버에서 눈 파내기를 실행한 후 반환하는 결과 묶음
struct FDRSnowRemovalExecutionResult
{
	FDRSnowRemoveResult RemoveResult;  // 실제 파낸 부피 및 팀 정보
	FDRSnowMaterialPatch MaterialPatch; // 파여서 새로 드러난 지형에 칠할 팀 색상 패치 (클라이언트 전송용)
};

// 클라이언트에서 눈 파내기를 재생/확정했을 때의 결과
struct FDRSnowRemovalReplayResult
{
	bool bApplied = false;                 // 지형이 실제로 변경되었는지 여부
	TWeakObjectPtr<AVoxelWorld> VoxelWorld; // 변경된 복셀 월드 포인터
};

// FDRSnowRemovalPipeline: 눈 파내기의 전체 실행 순서를 관리하는 파이프라인
//
// 핵심 실행 순서:
//   1. 지형 깎기 (SurfaceEditor)
//   2. 점령 부피 차감 및 소유권 갱신 (VolumeStore, OwnershipStore)
//   3. 새로 드러난 표면에 팀 색상 칠하기 (MaterialPatch 생성 또는 적용)
//
// 서버 실행과 클라이언트 예측 수명 주기를 모두 소유합니다. 클라이언트 서버 응답은
// ApplyServerUpdate 한 곳으로 들어오며, 내부에서 Fast Path 또는 선택적 rollback/replay를 결정합니다.
class DEEPRAIDERS_API FDRSnowRemovalPipeline
{
public:
	FDRSnowRemovalPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowOwnershipStore& InOwnershipStore,
		FDRSnowVolumeStore& InVolumeStore,
		const TSharedRef<FDRSnowMaterialPatchApplyQueue>& InMaterialPatchApplyQueue);

	// 서버: 지형 파기, 부피 차감, 팀 색상 계산을 모두 처리하고 결과(색상 패치 포함)를 반환합니다.
	FDRSnowRemovalExecutionResult Execute(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath,
		bool bBuildMaterialPatch);

	// 클라이언트: 입력 즉시 surface를 예측하고 서버 확인 대기열까지 내부에서 관리합니다.
	FDRSnowRemoveResult Predict(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath);

	// 클라이언트: 서버 확정 결과를 받아 Fast Path 또는 선택적 rollback/replay를 수행합니다.
	bool ApplyServerUpdate(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowServerResponse& Response);

	// authoritative add가 예측 surface와 충돌하지 않도록 전체 예측을 잠시 분리한 뒤 재적용합니다.
	FDRSnowAddResult ReconcileServerAdd(
		UWorld* World,
		FDRSnowAddPipeline& AddPipeline,
		const FDRSnowSurfaceAddRequest& Request,
		float AppliedAmount);

	void ResetPredictions();

private:
	struct FPendingRemovalPrediction
	{
		FDRSnowPredictionKey PredictionKey;
		FDRSnowSurfaceRemoveRequest Request;
		FDRSnowSurfaceEditResult SurfaceEdit;
		EDRSnowRemovalPath RemovalPath = EDRSnowRemovalPath::Standard;
		uint64 LocalOrder = 0;
	};

	FDRSnowSurfaceEditResult PredictSurface(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath);

	FDRSnowRemovalReplayResult Replay(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowServerResponse& Response);

	FDRSnowRemovalReplayResult ConfirmPrediction(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& PredictedSurfaceEdit,
		const FDRSnowServerResponse& Response);

	int32 FindMatchingPrediction(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;

	TArray<FPendingRemovalPrediction> SuspendPredictions();
	TArray<FPendingRemovalPrediction> SuspendPredictions(
		const TArray<int32>& PredictionIndices);
	void ResumePredictions(
		UWorld* World,
		TArray<FPendingRemovalPrediction>&& Predictions);

	FVoxelIntBox GetRequestBounds(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;
	TArray<int32> FindAffectedPredictionIndices(
		AVoxelWorld* VoxelWorld,
		const FVoxelIntBox& SeedBounds,
		int32 RequiredPredictionIndex = INDEX_NONE) const;

	// 파내기 방식(Standard / Absorb)에 따라 지형을 깎아냅니다.
	FDRSnowSurfaceEditResult RemoveSurface(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;

	// 파여서 새로 드러난 지형에 어떤 팀의 색상을 칠해야 하는지 주변 소유권을 검색해 결정합니다.
	bool ResolveMaterials(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		EDRSnowRemovalPath RemovalPath,
		FDRSnowResolvedMaterialEdit& OutResolvedEdit) const;

	// 색상 패치가 없는 이전 버전 데이터 수신 시, 로컬에서 직접 색상을 계산해 칠합니다.
	bool RepaintWithoutPatch(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		EDRSnowRemovalPath RemovalPath) const;

	// 지형 변경 결과에 맞춰 점령 부피를 차감하고, 서버인 경우 파여나간 복셀 소유권을 삭제합니다.
	void ApplyRemovedSurfaceEdit(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		float VolumeAmount);

	// 변경된 복셀별 높낮이 차이를 계산하여 VolumeStore에서 팀 점령량을 삭감합니다.
	void RemoveVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceRemoveRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxRemovedAmount);

	FDRSnowSurfaceEditor& SurfaceEditor;
	FDRSnowOwnershipStore& OwnershipStore;
	FDRSnowVolumeStore& VolumeStore;
	TSharedRef<FDRSnowMaterialPatchApplyQueue> MaterialPatchApplyQueue;
	TArray<FPendingRemovalPrediction> PendingPredictions;
	bool bPredictionCapacityWarningLogged = false;
	uint64 NextPredictionOrder = 0;
};
