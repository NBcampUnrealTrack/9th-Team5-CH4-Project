#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DRSnowSubsystem.generated.h"

class AVoxelWorld;
class FDRSnowAddPipeline;
class FDRSnowMaterialPatchApplyQueue;
class FDRSnowRemovalPipeline;
class FDRSnowVoxelContainmentEvaluator;
enum class EDRSnowRemovalPath : uint8;

// UDRSnowSubsystem: 게임 내 눈 지형 및 점령 시스템의 메인 창구
//
// 외부(GameState, GAS 등)는 오직 이 서브시스템만 호출합니다.
// 내부의 복셀 외형(Surface), 팀별 점령량(Volume), 팀 색상(Ownership), 난입 동기화(Snapshot)는
// 서브시스템과 각 전용 파이프라인 내부에서 조율됩니다.
//
// 기본 동기화 흐름:
//   서버: AddSnow, RemoveSnow 실행 -> GameState를 통해 Multicast 배치 전송
//   클라이언트: PredictSnowRemoval로 즉각 지형을 먼저 파내고, 서버 RPC 수신 시 점령량과 색상을 확정
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
		float AppliedAmount);

	// 일반 눈 파내기
	// 서버: 지정 반경의 눈을 파내고, 점령 부피 삭감 및 드러난 표면의 색상 패치(OutMaterialPatch)를 생성합니다.
	FDRSnowRemoveResult RemoveSnow(
		const FDRSnowSurfaceRemoveRequest& Request,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);

	// 클라이언트 예측: 지연 시간 없이 반응하도록 외형(Surface)만 먼저 파냅니다.
	// 실제 점령량과 표면 색상은 서버 확정 수신 시 동기화됩니다.
	FDRSnowRemoveResult PredictSnowRemoval(
		const FDRSnowSurfaceRemoveRequest& Request);

	// 클라이언트 복제: 서버 확정 데이터를 받아 예측과 대조 후 확정하거나, 처음부터 다시 적용합니다.
	bool ApplyReplicatedSnowRemoval(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch = nullptr);

	// 눈총 흡수 전용 파내기
	// 서버: 원뿔 시야(Frustum) 형태로 눈을 흡수하며, 일반 파내기와 다른 재질 보정 방식을 사용합니다.
	FDRSnowRemoveResult RemoveSnowWithAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request,
		FDRSnowMaterialPatch* OutMaterialPatch = nullptr);

	// 클라이언트 예측: 눈총 흡수 시 시각적 지형을 즉시 파냅니다.
	FDRSnowRemoveResult PredictSnowAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request);

	// 클라이언트 복제: 눈총 흡수 결과를 로컬 상태에 동기화합니다.
	bool ApplyReplicatedSnowAbsorbTool(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AppliedAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch = nullptr);

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
		const TArray<uint8>& SnowVolumeData);

private:
	// 클라이언트 선행 예측 보관 구조체
	// 클라이언트가 먼저 파낸 지형 결과(SurfaceEdit)를 들고 있다가, 서버에서 같은 키(PredictionKey)의
	// RPC가 도착하면 지형을 다시 파지 않고 서버 확정 수치와 색상 패치만 덮어씌웁니다.
	struct FPendingRemovalPrediction
	{
		FDRSnowPredictionKey PredictionKey;
		FDRSnowSurfaceRemoveRequest Request;
		FDRSnowSurfaceEditResult SurfaceEdit;
		EDRSnowRemovalPath RemovalPath;
		uint64 LocalOrder = 0;
	};

	FDRSnowRemoveResult PredictSnowRemovalInternal(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath);

	// 서버 RPC와 일치하는 예측 결과의 인덱스를 반환합니다.
	int32 FindMatchingRemovalPrediction(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;

	// authoritative add처럼 정확한 영향 경계를 알 수 없는 경로에서 전체 예측을 분리합니다.
	TArray<FPendingRemovalPrediction> SuspendRemovalPredictions();

	// 선택한 예측만 역순 롤백하고 나머지는 현재 geometry에 그대로 유지합니다.
	TArray<FPendingRemovalPrediction> SuspendRemovalPredictions(
		const TArray<int32>& PredictionIndices);

	FVoxelIntBox GetRemovalRequestBounds(
		const FDRSnowSurfaceRemoveRequest& Request,
		EDRSnowRemovalPath RemovalPath) const;

	// seed와 공간적으로 연결된 예측들의 전이적 묶음을 찾습니다.
	TArray<int32> FindAffectedRemovalPredictionIndices(
		AVoxelWorld* VoxelWorld,
		const FVoxelIntBox& SeedBounds,
		int32 RequiredPredictionIndex = INDEX_NONE) const;

	// authoritative 작업 이후 아직 응답받지 않은 예측을 원래 순서대로 다시 적용합니다.
	void ResumeRemovalPredictions(TArray<FPendingRemovalPrediction>&& Predictions);

	bool ApplyReplicatedSnowRemovalInternal(
		const FDRSnowSurfaceRemoveRequest& Request,
		float AuthoritativeAmount,
		const FDRSnowMaterialPatch* AuthoritativeMaterialPatch,
		EDRSnowRemovalPath RemovalPath);

	// 게임 리셋 시 대기 중이던 예측 목록을 비웁니다.
	void ResetRemovalPredictions();

	// 서버 전용: 각 복셀의 팀 색상(MaterialIndex) 원본 저장소 (32^3 청크 단위)
	FDRSnowOwnershipStore OwnershipStore;

	// 팀별 눈 점유 부피 저장소 (UI 점령 비율 산출 및 승패 판정 기준)
	FDRSnowVolumeStore VolumeStore;

	// 복셀 플러그인을 직접 제어하여 실제 지형을 깎거나 쌓는 도구
	FDRSnowSurfaceEditor SurfaceEditor;

	// 지형이 변했을 때 플레이어가 눈 속에 묻혔는지 감지하는 판정기
	TSharedPtr<FDRSnowVoxelContainmentEvaluator> ContainmentEvaluator;

	// 난입 플레이어용 맵 상태 압축 및 복원 직렬화기
	TUniquePtr<FDRSnowSnapshotSerializer> SnapshotSerializer;

	// 눈 파내기 단계별 조율자 (지형 파기 -> 부피 삭감 -> 색상 재도색)
	TSharedPtr<FDRSnowRemovalPipeline> RemovalPipeline;

	// 눈 쌓기 단계별 조율자 (지형 생성 -> 부피 누적 -> 색상 적용)
	TSharedPtr<FDRSnowAddPipeline> AddPipeline;

	// 네트워크로 받은 팀 색상 패치를 차례대로 안전하게 렌더링에 적용하는 큐
	TSharedPtr<FDRSnowMaterialPatchApplyQueue> MaterialPatchApplyQueue;

	// 서버 확인을 기다리는 클라이언트 예측 목록 (최대 32개)
	TArray<FPendingRemovalPrediction> PendingRemovalPredictions;
	bool bPredictionCapacityWarningLogged = false;
	uint64 NextRemovalPredictionOrder = 0;
	int32 SnowStateGeneration = 0;
};
