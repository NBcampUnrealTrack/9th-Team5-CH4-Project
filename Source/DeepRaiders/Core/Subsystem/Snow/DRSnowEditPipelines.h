#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"

class AVoxelWorld;
class FDRSnowSurfaceEditor;
class FDRSnowVolumeStore;
class UWorld;
struct FModifiedVoxelValue;
struct FDRSnowSurfaceEditResult;

// FDRSnowAddPipeline: 눈 쌓기의 전체 실행 순서를 관리하는 파이프라인
//
// 기본 순서:
//   1. 지형 생성 (SurfaceEditor)
//   2. 점령 부피 가산 (VolumeStore)
//   3. 추가한 내부 복셀의 팀 재질 저장 (SurfaceEditor)
//   4. 캐릭터 파묻힘 판정 (ContainmentEvaluator)
//
// 도구별 특성:
//   - 눈벽 (OrientedBoxTool): 지형을 먼저 만든 뒤 실제 생성된 크기만큼 점령 부피를 기록합니다.
//   - 방향성 눈 (DirectionalSurfaceTool): 지형을 먼저 쏘아올리며, 클라이언트는 서버 확정 수치를 사용합니다.
//   - 일반 눈: 점령 부피를 먼저 검증하고 지형을 쌓습니다.
class DEEPRAIDERS_API FDRSnowAddPipeline : public TSharedFromThis<FDRSnowAddPipeline>
{
public:
	FDRSnowAddPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowVolumeStore& InVolumeStore);

	// 서버: 도구 종류에 맞게 지형 생성, 점령 부피 누적, 팀 색상 등록을 일괄 수행합니다.
	FDRSnowAddResult Execute(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request,
		TFunction<void(float)> DirectionalCompletion = {});

	// 클라이언트 재생: 서버의 눈 추가 명령을 수신하여 로컬에 재현합니다.
	// 방향성 눈은 서버가 확정한 실제 양(AuthoritativeAmount)을 부피에 반영합니다.
	FDRSnowAddResult Replay(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request,
		float AuthoritativeAmount,
		TFunction<void(float)> DirectionalCompletion = {});

	// 게임 상태 초기화 시 대기 중인 비동기 작업을 폐기하고 실행 중인 콜백을 무효화합니다.
	void Reset(int32 NewStateGeneration);

private:
	struct FPendingDirectionalAdd
	{
		TWeakObjectPtr<UWorld> World;
		FDRSnowSurfaceAddRequest Request;
		TOptional<float> AuthoritativeAmount;
		TFunction<void(float)> Completion;
		int32 StateGeneration = 0;
	};

	void ProcessNextDirectionalAdd();
	void HandleDirectionalAddCompleted(
		TWeakObjectPtr<UWorld> World,
		const FDRSnowSurfaceAddRequest& Request,
		TOptional<float> AuthoritativeAmount,
		int32 RequestGeneration,
		TFunction<void(float)> Completion,
		FDRSnowSurfaceEditResult&& EditResult);

	// 지형 생성이 완료된 후 내부 재질 저장, 부피 갱신, 캐릭터 파묻힘 검사를 순서대로 진행합니다.
	void CommitAddedSurfaceEdit(
		UWorld* World,
		const FDRSnowSurfaceAddRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		TOptional<float> VolumeAmount = {});

	// 실제로 추가된 복셀들의 높낮이 차이를 계산하여 VolumeStore에 팀 점령량으로 누적합니다.
	void AddVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceAddRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxAddedAmount);

	FDRSnowSurfaceEditor& SurfaceEditor;
	FDRSnowVolumeStore& VolumeStore;

	TQueue<FPendingDirectionalAdd> PendingDirectionalAdds;
	int32 CurrentStateGeneration = 0;
	bool bDirectionalAddInProgress = false;
};

enum class EDRSnowRemovalPath : uint8
{
	Standard,
	Absorb
};

// 눈 제거의 전체 도메인 순서를 소유한다.
// Surface 편집 → 점령 부피 갱신 순서가 이 클래스 밖으로 흩어지지 않는다.
class DEEPRAIDERS_API FDRSnowRemovalPipeline
{
public:
	FDRSnowRemovalPipeline(
		FDRSnowSurfaceEditor& InSurfaceEditor,
		FDRSnowVolumeStore& InVolumeStore);

	FDRSnowRemoveResult Execute(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath);

	bool Replay(
		UWorld* World,
		const FDRSnowSurfaceRemoveRequest& Request,
		float AuthoritativeAmount,
		EDRSnowRemovalPath RemovalPath);

private:
	FDRSnowSurfaceEditResult RemoveSurface(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;
	void ApplyRemovedSurfaceEdit(
		const FDRSnowSurfaceRemoveRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult,
		float VolumeAmount);
	void RemoveVolumeFromModifiedValues(
		AVoxelWorld& VoxelWorld,
		const FDRSnowSurfaceRemoveRequest& Request,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		float MaxRemovedAmount);

	FDRSnowSurfaceEditor& SurfaceEditor;
	FDRSnowVolumeStore& VolumeStore;
};
