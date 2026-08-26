#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelTerrainOperationLibrary.h"
#include "DRVoxelTerrainSyncLibrary.h"
#include "DRVoxelTerrainAreaSyncActor.generated.h"

class AVoxelWorld;
class UDRVoxelTerrainSubsystem;

// 지정 영역의 퇴적 요청 처리, 편집 델타 복제, 클라이언트 재생을 한 곳에서 관리한다.
// 권한 서버만 지형을 생성하고 기록하며 클라이언트는 Revision 순서대로 기록을 재생한다.
UCLASS()
class DEEPRAIDERS_API ADRVoxelTerrainAreaSyncActor : public AActor
{
	GENERATED_BODY()

public:
	ADRVoxelTerrainAreaSyncActor();
	virtual ~ADRVoxelTerrainAreaSyncActor() override;

	// 에디터에서 위치나 프로퍼티를 바꿀 때 청크 미리보기를 즉시 다시 만든다.
	virtual void OnConstruction(const FTransform& Transform) override;
	// 서버는 진행 중 퇴적 요청을 처리하고, 클라이언트는 도착한 델타를 순서대로 재생한다.
	virtual void Tick(float DeltaSeconds) override;
	// VoxelWorld 참조와 편집 Revision/델타 배열을 네트워크 복제 대상으로 등록한다.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	// PIE를 시작하지 않아도 에디터 뷰포트에서 격자점과 청크 박스를 계속 표시한다.
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

	// 현재 월드 박스와 청크 크기를 기준으로 XY 표면 처리 경계를 다시 계산한다.
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Voxel Terrain|Sync|Chunks")
	void RebuildTerrainChunks();

protected:
	// 서버 전용 타이머와 굴착 델리게이트를 구성한다. 클라이언트는 이 작업을 실행하지 않는다.
	virtual void BeginPlay() override;
	// 액터 종료 시 서브시스템 델리게이트 바인딩을 해제해 죽은 UObject 호출을 방지한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 실제 데이터를 읽고 쓰는 대상이다. 클라이언트도 같은 월드를 찾아 델타를 재생해야 하므로 복제한다.
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_VoxelWorld, BlueprintReadOnly, Category="Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	// 액터 위치를 중심으로 하는 월드 공간 반크기다. 스캔, 퇴적, 굴착 필터, 청크 표시가 모두 이 영역을 공유한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	FVector BoxExtent = FVector(500.f, 500.f, 500.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	FColor TerrainChunkBoxColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug", meta=(ClampMin="0.1"))
	float TerrainChunkBoxThickness = 1.5f;

	// 청크 수가 큰 영역에서도 디버그 드로우가 프레임을 과도하게 점유하지 않도록 표시 개수를 제한한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug", meta=(ClampMin="1"))
	int32 MaxDebugTerrainChunkBoxes = 512;

	// 새 퇴적 요청을 만들려고 시도하는 서버 타이머 간격이다. 요청 처리 자체는 매 Tick 수행된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.01"))
	float DepositInterval = 1.f;

	// false로 바꾸면 진행 중 요청도 취소하고 더 이상 퇴적 요청을 만들지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bEnableDepositAccumulation = false;

	// 새 요청마다 복사되는 퇴적 형태/확률 설정이다. RandomSeed는 요청 생성 시 별도로 무작위 값으로 교체한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	FDRVoxelDepositInBoxSettings DepositSettings;

	// 관리 박스를 XY로 나누는 한 청크의 월드 단위 한 변 길이다. 각 청크는 BoxExtent의 전체 Z를 공유한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="1.0"))
	float DepositChunkWorldSize = 2000.f;

	// 스캔 열, 복셀 쓰기 시도, 비동기 트레이스 예산을 하나의 프리셋으로 묶어 세부 옵션 노출을 줄인다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	EDRDepositPerformancePreset PerformancePreset = EDRDepositPerformancePreset::Balanced;

	// true면 복셀 지형뿐 아니라 관리 박스 안의 고정 StaticMesh 표면도 퇴적 지지면으로 사용한다.
	// 메시 위에 별도 오브젝트를 생성하지 않고, 서버가 찾은 표면 높이를 기존 복셀 퇴적 요청에 합친다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bDepositOnStaticMeshes = false;

	// true면 StaticMesh 위 퇴적 사용 여부와 관계없이 위쪽 WorldStatic 충돌을 천장으로 취급한다.
	// 복셀 지형 후보가 지붕/천장 아래에서 선택되거나 풋프린트가 실내로 번지는 것을 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bBlockDepositBelowStaticMeshes = true;

	// 이 각도보다 가파른 고정 메시 표면은 퇴적 지지면에서 제외한다. 내부에서는 노멀 Z 기준으로 변환한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh", meta=(ClampMin="0.0", ClampMax="90.0"))
	float MaxStaticMeshSlopeAngle = 50.f;

	// None이면 모든 고정 StaticMesh를 허용한다. 이름을 지정하면 컴포넌트 또는 소유 액터에
	// 같은 태그가 있는 메시만 지지면으로 인정해 장식 메시 등을 간단히 제외할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh")
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	// true면 렌더 삼각형을 기준으로 정밀하게 표면을 찾는다. 단순 충돌보다 정확하지만 트레이스 비용이 커질 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bTraceComplexStaticMeshSurfaces = false;

	// 0이면 체크포인트가 없는 현재 구조에서 중도 난입 플레이어가 처음부터 재생할 수 있도록 전체 기록을 유지한다.
	// 양수로 제한하면 오래된 Revision이 삭제되므로, 해당 값을 사용하려면 별도의 스냅샷 또는 체크포인트 동기화가 필요하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	int32 MaxReplicatedDeltaRecords = 0;

	// 현재 액터 설정으로 계산된 2D 표면 청크다. 런타임 저장이나 네트워크 복제 대상은 아니다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Voxel Terrain|Sync|Chunks")
	TArray<FDRVoxelTerrainChunkBounds> TerrainChunks;

	// 퇴적과 굴착 모두 이 번호를 공유하며 FastArray 항목은 항상 이 번호의 오름차순으로 추가된다.
	UPROPERTY(ReplicatedUsing=OnRep_TerrainDeltaState, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	int32 TerrainEditRevision = 0;

	// 퇴적과 굴착을 하나의 FastArray로 복제해 새 항목/삭제분만 전송하고 타입 간 도착 순서 병합을 없앤다.
	UPROPERTY(Replicated)
	FDRTerrainEditFastArray TerrainEdits;

	// 이전 Blueprint 자산의 프로퍼티 참조를 깨지 않기 위한 비복제 호환 필드다. 새 기록은 TerrainEdits만 사용한다.
	UPROPERTY(Transient, BlueprintReadOnly, Category="Voxel Terrain|Sync", meta=(DeprecatedProperty, DeprecationMessage="Use the unified terrain edit stream."))
	TArray<FDRVoxelDepositDeltaRecord> DepositDeltaRecords;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Voxel Terrain|Sync", meta=(DeprecatedProperty, DeprecationMessage="Use the unified terrain edit stream."))
	TArray<FDRVoxelDigDeltaRecord> DigDeltaRecords;

	// VoxelWorld 참조가 도착하면 청크 경계를 갱신하고, 먼저 도착해 있던 델타의 재생을 다시 시도한다.
	UFUNCTION()
	void OnRep_VoxelWorld();

	// 서버의 최종 Revision이 먼저 도착한 경우에도 현재 FastArray 자료만으로 안전하게 재생을 시도한다.
	UFUNCTION()
	void OnRep_TerrainDeltaState();

public:
	// FastArray의 추가/변경 콜백이 새 편집을 즉시 재생할 수 있게 하는 진입점이다.
	void HandleReplicatedTerrainEdits();

private:
	// 서버 퇴적 패스와 StaticMesh 비동기 스캔 상태를 액터 인스턴스별로 소유한다.
	// 상태 전이는 TerrainOperationLibrary만 수행하므로 Actor는 내부 단계에 의존하지 않는다.
	FDRVoxelTerrainOperationState TerrainOperationState;

	// 액터가 생명주기를 소유하고 정적 동기화 라이브러리는 명시적으로 전달받은 상태만 갱신한다.
	FDRTerrainSyncServerState TerrainSyncServerState;
	FDRTerrainSyncClientState TerrainSyncClientState;

	// 서버 타이머 핸들과 굴착 델리게이트 생명주기를 보관한다.
	FTimerHandle DepositTimerHandle;
	TWeakObjectPtr<UDRVoxelTerrainSubsystem> TerrainSubsystem;
	FDelegateHandle TerrainDugDelegateHandle;

	// 매 Tick 전체 청크 배열을 다시 만들지 않도록 마지막 계산 입력을 저장한 캐시다.
	FVector CachedChunkCenter = FVector::ZeroVector;
	FVector CachedChunkExtent = FVector::ZeroVector;
	float CachedDepositChunkWorldSize = 0.f;
	bool bHasCachedChunkLayout = false;

	// 위치, 크기, 월드 변환 등 청크 입력이 바뀐 경우에만 경계를 다시 계산한다.
	void EnsureTerrainChunksCurrent();
	// 지형 라이브러리에 필요한 현재 설정과 월드 참조만 모아 명시적인 호출 컨텍스트를 만든다.
	FDRVoxelTerrainOperationContext MakeTerrainOperationContext();
	// 진행 중 패스가 없을 때 전체 청크 순환을 시작한다.
	void RequestDepositArea();
	// 기능 비활성화, 월드 교체, 청크 레이아웃 변경 시 현재 패스와 요청 상태를 함께 폐기한다.
	void CancelDepositPass();
	// 대기 변경을 고정 32^3 청크/머터리얼별 FastArray 항목으로 확정한다.
	void FlushPendingDepositBatch();
	// 새 통합 편집 항목을 추가하고 FastArray에 dirty 표시한다.
	void AddTerrainEditItem(FDRTerrainEditFastArrayItem&& Item);
	// 클라이언트에서 통합 FastArray를 Revision 순서로 재생한다.
	void ApplyPendingDeltaRecords();
	// 설정된 Revision 보존 범위보다 오래된 통합 기록을 제거한다.
	void TrimReplicatedTerrainRecords();
	// 서버 TerrainSubsystem의 굴착 완료 이벤트를 구독/해제한다.
	void BindTerrainDugDelegate();
	void UnbindTerrainDugDelegate();
	// 관리 박스와 겹친 굴착을 공통 Revision 기록으로 변환한다.
	void HandleTerrainDug(const FVector& Location, float Radius);
	// 아래 함수들은 게임 데이터에 영향을 주지 않는 에디터/런타임 디버그 표시다.
	void DrawTerrainChunkBoxes() const;
};
