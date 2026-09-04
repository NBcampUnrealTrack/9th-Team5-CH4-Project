#pragma once

#include "CoreMinimal.h"
#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

class AVoxelWorld;
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
// 상황별 4가지 실행 함수:
//   Execute: 서버에서 위 1, 2, 3 단계를 모두 수행하고 클라이언트용 색상 패치 생성
//   PredictSurface: 클라이언트에서 입력 즉시 1단계(지형 깎기)만 먼저 수행하여 렉을 숨김
//   ConfirmPrediction: 예측과 서버 결과가 일치할 때 geometry를 유지하고 2, 3단계만 확정
//   Replay: 로컬 예측을 롤백한 authoritative 상태에서 1, 2, 3단계를 순서대로 재현
class DEEPRAIDERS_API FDRSnowRemovalPipeline
{
public:
	FDRSnowRemovalPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowOwnershipStore& InOwnershipStore,
		FDRSnowVolumeStore& InVolumeStore);

	// 서버: 지형 파기, 부피 차감, 팀 색상 계산을 모두 처리하고 결과(색상 패치 포함)를 반환합니다.
	FDRSnowRemovalExecutionResult Execute(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath,
		bool bBuildMaterialPatch);

	// 클라이언트 예측: 입력 즉시 시각적인 지형만 깎아내고, 변경된 복셀 목록을 반환합니다.
	FDRSnowSurfaceEditResult PredictSurface(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath);

	// 클라이언트 재생: 서버 수신 데이터를 바탕으로 지형부터 색상까지 처음부터 적용합니다.
	FDRSnowRemovalReplayResult Replay(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		float AuthoritativeAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch,
		EDRSnowRemovalPath RemovalPath);

	// 예측한 geometry가 서버 결과와 일치할 때 지형을 다시 편집하지 않고
	// 점령 부피와 material만 확정하는 빠른 경로입니다.
	FDRSnowRemovalReplayResult ConfirmPrediction(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& PredictedSurfaceEdit,
		float AuthoritativeAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch,
		EDRSnowRemovalPath RemovalPath);

private:
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
};
