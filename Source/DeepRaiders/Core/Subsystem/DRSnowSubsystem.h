#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DRSnowSubsystem.generated.h"

class AVoxelWorld;
class FDRSnowAddPipeline;
class FDRSnowRemovalPipeline;

// UDRSnowSubsystem: 게임 내 눈 지형 및 점령 시스템의 메인 창구
//
// 외부(GameState, GAS 등)는 오직 이 서브시스템만 호출합니다.
// 내부의 복셀 외형(Surface), 팀별 점령량(Volume), 난입 동기화(Snapshot)는
// 서브시스템과 각 전용 파이프라인 내부에서 조율됩니다.
//
// 기본 동기화 흐름:
//   서버: AddSnow, RemoveSnow 실행 -> GameState를 통해 Multicast 배치 전송
//   클라이언트: 서버에서 확정된 작업을 수신한 뒤 지형, 점령량, 색상을 적용
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

	// 눈 추가
	// 서버: 복셀 지형을 쌓고, 팀 점령 부피와 색상(Material)을 기록합니다.
	FDRSnowAddResult AddSnow(
		const FDRSnowSurfaceAddRequest& Request,
		TFunction<void(float)> DirectionalCompletion = {});

	// 클라이언트: 서버에서 확정된 눈 추가 작업을 수신하여 로컬 상태에 반영합니다.
	FDRSnowAddResult ApplyReplicatedSnowAdd(
		const FDRSnowSurfaceAddRequest& Request,
		float AppliedAmount,
		TFunction<void(float)> DirectionalCompletion = {});

	// 일반 눈 파내기
	// 서버: 지정 반경의 눈을 파내고 점령 부피를 삭감합니다.
	FDRSnowRemoveResult RemoveSnow(
		const FDRSnowSurfaceRemoveRequest& Request);

	// 클라이언트 복제: 서버 확정 데이터를 받아 지형과 점령량을 적용합니다.
	bool ApplyReplicatedSnowRemoval(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount);

	// 눈총 흡수 전용 파내기
	// 서버: 원뿔 시야(Frustum) 형태로 눈을 흡수합니다.
	FDRSnowRemoveResult RemoveSnowWithAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request);

	// 점령 상태 조회
	// 특정 위치에서 가장 많은 지분을 가진 팀 ID 반환
	int32 GetDominantTeamAtLocation(FVector WorldLocation) const;
	// 특정 영역 내 팀별 눈 점유 비율 조회 (UI 게이지 등에 사용)
	FDRSnowControlRatio QuerySnowInBounds(const FBox& WorldBounds) const;

	// 중도 난입 플레이어 동기화
	FDRJoinSnapshotSizeReport MeasureCompressedSnapshotSize(AVoxelWorld* TargetVoxelWorld = nullptr, bool bLogResult = true);
	bool CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld = nullptr);
	bool GetLatestCheckpointOperationSequence(int32& OutOperationSequence);
	bool GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint);
	bool GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Snow|Snapshot")
	void ResetCheckpoints();

	// 새 게임 시작 시 눈 데이터와 체크포인트를 초기화합니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Snow")
	void ResetSnowState();

	// 난입한 클라이언트가 서버의 복셀 지형과 점령 부피를 한 번에 복원합니다.
	bool ApplyCheckpoint(
		FName VoxelWorldName,
		const TArray<uint8>& VoxelSaveData,
		int32 OriginalVoxelSaveSize,
		const TArray<uint8>& SnowVolumeData,
		int32 OriginalSnowVolumeSize);

private:

	// 팀별 눈 점유 부피 저장소 (UI 점령 비율 산출 및 승패 판정 기준)
	FDRSnowVolumeStore VolumeStore;

	// 복셀 플러그인을 직접 제어하여 실제 지형을 깎거나 쌓는 도구
	FDRSnowSurfaceEditor SurfaceEditor;

	// 난입 플레이어용 맵 상태 압축 및 복원 직렬화기
	TUniquePtr<FDRSnowSnapshotSerializer> SnapshotSerializer;

	// 눈 파내기 단계별 조율자 (지형 파기 -> 부피 삭감)
	TSharedPtr<FDRSnowRemovalPipeline> RemovalPipeline;

	// 눈 쌓기 단계별 조율자 (지형 생성 -> 부피 누적 -> 색상 적용)
	TSharedPtr<FDRSnowAddPipeline> AddPipeline;

	int32 SnowStateGeneration = 0;
};
