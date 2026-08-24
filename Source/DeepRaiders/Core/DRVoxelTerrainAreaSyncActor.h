#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelTerrainQueryLibrary.h"
#include "WorldCollision.h"
#include "DRVoxelTerrainAreaSyncActor.generated.h"

class AVoxelWorld;
class UDRVoxelTerrainSubsystem;

// 동기화 대상 박스를 고정 크기 복셀 청크로 나눈 결과다.
// 현재는 네트워크 패킷 분할이 아니라 영역 확인과 디버그 표시를 위한 경계 정보로 사용한다.
USTRUCT(BlueprintType)
struct FDRVoxelTerrainChunkBounds
{
	GENERATED_BODY()

	// 동기화 박스의 VoxelMin을 원점으로 한 청크 X/Y/Z 번호다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FIntVector ChunkCoordinate = FIntVector::ZeroValue;

	// 이 청크가 포함하는 첫 로컬 복셀 좌표다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	// 반복문과 크기 계산을 단순하게 하기 위한 미포함 최댓값이다. 유효 범위는 [VoxelMin, VoxelMaxExclusive)다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FIntVector VoxelMaxExclusive = FIntVector::ZeroValue;
};

// 지정 영역의 퇴적 요청 처리, 편집 델타 복제, 클라이언트 재생을 한 곳에서 관리한다.
// 권한 서버만 지형을 생성하고 기록하며 클라이언트는 Revision 순서대로 기록을 재생한다.
UCLASS()
class DEEPRAIDERS_API ADRVoxelTerrainAreaSyncActor : public AActor
{
	GENERATED_BODY()

public:
	ADRVoxelTerrainAreaSyncActor();

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

	// 현재 박스와 VoxelWorld 설정을 기준으로 디버그용 청크 경계를 다시 계산한다.
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

	// 재료 통계 스캔에서 집계할 인덱스 목록이다. 비어 있으면 모든 머터리얼을 집계한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	TArray<uint8> TeamMaterialIndices;

	// 통계 스캔은 3중 반복 비용이 크므로 게임 로직에서 필요할 때만 명시적으로 활성화한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	bool bEnableMaterialCountScan = false;

	// 재료 통계 타이머 간격이다. 퇴적 요청 주기와는 별개다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain", meta=(ClampMin="0.01"))
	float ScanInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	bool bDrawDebugBox = true;

	// 퇴적 샘플의 기준 3D 격자를 표시한다. 실제 샘플에는 지터가 더해지므로 최종 위치와 정확히 일치하지는 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	bool bDrawDepositGridPoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug", meta=(ClampMin="1.0"))
	float DepositGridPointSize = 6.f;

	// 에디터 렌더링 부담을 제한하기 위해 한 프레임에 표시할 격자점 수를 제한한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug", meta=(ClampMin="1"))
	int32 MaxDebugDepositGridPoints = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	FColor DepositGridPointColor = FColor::White;

	// RebuildTerrainChunks가 만든 복셀 청크 경계를 월드 공간 박스로 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	bool bDrawTerrainChunkBoxes = false;

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

	// 한 Tick의 읽기 단계에서 조사할 X/Y 열 상한이다. 낮추면 프레임 부하는 줄지만 한 요청 완료가 느려진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="1"))
	int32 MaxDepositScanColumnsPerTick = 32;

	// 한 Tick의 쓰기 단계에서 검사할 풋프린트 칸 상한이다. 실제 수정 수가 아니라 실패 검사를 포함한 시도 횟수다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="1"))
	int32 MaxDepositVoxelWriteAttemptsPerTick = 128;

	// true면 복셀 지형뿐 아니라 관리 박스 안의 고정 StaticMesh 표면도 퇴적 지지면으로 사용한다.
	// 메시 위에 별도 오브젝트를 생성하지 않고, 서버가 찾은 표면 높이를 기존 복셀 퇴적 요청에 합친다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bDepositOnStaticMeshes = false;

	// 한 Tick에 새로 발행할 비동기 하향 트레이스 수다. 낮추면 표면 스캔 시간이 늘어나는 대신
	// Chaos 쿼리 제출 비용이 여러 프레임으로 더 고르게 분산된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh", meta=(ClampMin="1"))
	int32 MaxStaticMeshTraceRequestsPerTick = 32;

	// 완료를 기다리는 비동기 트레이스의 최대 개수다. 물리 스레드가 늦어져도 요청이 무제한 누적되지 않게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh", meta=(ClampMin="1"))
	int32 MaxPendingStaticMeshTraces = 64;

	// 월드 위쪽을 향하는 노멀의 최소 Z다. 값이 클수록 수직 벽과 급경사면에는 쌓이지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float MinStaticMeshSurfaceNormalZ = 0.65f;

	// None이면 모든 고정 StaticMesh를 허용한다. 이름을 지정하면 컴포넌트 또는 소유 액터에
	// 같은 태그가 있는 메시만 지지면으로 인정해 장식 메시 등을 간단히 제외할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	// true면 렌더 삼각형을 기준으로 정밀하게 표면을 찾는다. 단순 충돌보다 정확하지만 트레이스 비용이 커질 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bTraceComplexStaticMeshSurfaces = false;

	// 0이면 체크포인트가 없는 현재 구조에서 중도 난입 플레이어가 처음부터 재생할 수 있도록 전체 기록을 유지한다.
	// 양수로 제한하면 오래된 Revision이 삭제되므로, 해당 값을 사용하려면 별도의 스냅샷 또는 체크포인트 동기화가 필요하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	int32 MaxReplicatedDeltaRecords = 0;

	// 디버그/향후 공간 분할 기준이 되는 한 청크의 축별 복셀 크기다. 현재 델타 배열 자체를 청크별 복제하지는 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks", meta=(ClampMin="1"))
	int32 TerrainChunkSizeInVoxels = 16;

	// 현재 액터 설정으로 계산된 청크 경계 캐시다. 런타임 저장이나 네트워크 복제 대상은 아니다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Voxel Terrain|Sync|Chunks")
	TArray<FDRVoxelTerrainChunkBounds> TerrainChunks;

	// 퇴적과 굴착 모두 이 번호를 공유한다. 두 배열이 서로 다른 네트워크 프레임에 도착해도
	// 클라이언트가 Revision을 기준으로 병합하면 서버에서 발생한 지형 편집 순서를 그대로 복원할 수 있다.
	UPROPERTY(ReplicatedUsing=OnRep_TerrainDeltaState, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	int32 TerrainEditRevision = 0;

	// 서버에서 실제로 값이 바뀐 퇴적 배치만 누적한다. OnRep에서 굴착 배열과 Revision 순서로 병합한다.
	UPROPERTY(ReplicatedUsing=OnRep_TerrainDeltaState, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	TArray<FDRVoxelDepositDeltaRecord> DepositDeltaRecords;

	// 관리 영역과 겹친 서버 굴착 이벤트를 누적한다. 위치/반지름만으로 클라이언트 RemoveSphere를 재생한다.
	UPROPERTY(ReplicatedUsing=OnRep_TerrainDeltaState, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	TArray<FDRVoxelDigDeltaRecord> DigDeltaRecords;

	// VoxelWorld 참조가 도착하면 청크 경계를 갱신하고, 먼저 도착해 있던 델타의 재생을 다시 시도한다.
	UFUNCTION()
	void OnRep_VoxelWorld();

	// Revision 또는 두 델타 배열 중 어느 프로퍼티가 도착해도 동일한 병합/재생 경로를 실행한다.
	UFUNCTION()
	void OnRep_TerrainDeltaState();

private:
	// 서버에서만 사용하는 틱 분할 요청 큐다. 현재 액터는 진행 중 요청이 있으면 새 요청을 추가하지 않는다.
	UPROPERTY()
	TArray<FDRVoxelDepositInBoxRequest> DepositRequests;

	// 비동기 트레이스는 발행한 다음 프레임부터 결과를 조회할 수 있다. 진행 중 핸들과 유효 히트 위치를
	// 요청 하나의 런타임 상태로 보관하고, 모두 끝난 뒤 한 번에 복셀 퇴적 요청으로 넘긴다.
	TArray<FTraceHandle> PendingStaticMeshTraceHandles;
	TArray<FVector> StaticMeshSurfaceHitPositions;
	TArray<int32> StaticMeshTraceColumnOrder;
	FRandomStream StaticMeshTraceRandomStream;
	TWeakObjectPtr<AVoxelWorld> StaticMeshScanVoxelWorld;
	FVector StaticMeshScanCenter = FVector::ZeroVector;
	FVector StaticMeshScanExtent = FVector::ZeroVector;
	int32 StaticMeshTraceColumnCountY = 0;
	int32 NextStaticMeshTraceColumnIndex = 0;
	bool bStaticMeshSurfaceScanActive = false;

	// 클라이언트가 마지막으로 성공적으로 적용한 Revision과 중복 경고 방지용 누락 Revision이다.
	int32 LastAppliedTerrainRevision = 0;
	int32 LastReportedMissingRevision = 0;

	// 서버 타이머 핸들과 굴착 델리게이트 생명주기를 보관한다.
	FTimerHandle ScanTimerHandle;
	FTimerHandle DepositTimerHandle;
	TWeakObjectPtr<UDRVoxelTerrainSubsystem> TerrainSubsystem;
	FDelegateHandle TerrainDugDelegateHandle;

	// 매 Tick 전체 청크 배열을 다시 만들지 않도록 마지막 계산 입력을 저장한 캐시다.
	TWeakObjectPtr<AVoxelWorld> CachedChunkVoxelWorld;
	FTransform CachedChunkVoxelWorldTransform = FTransform::Identity;
	FVector CachedChunkCenter = FVector::ZeroVector;
	FVector CachedChunkExtent = FVector::ZeroVector;
	float CachedChunkVoxelSize = 0.f;
	int32 CachedChunkSizeInVoxels = 0;
	bool bHasCachedChunkLayout = false;

	// 위치, 크기, 월드 변환 등 청크 입력이 바뀐 경우에만 경계를 다시 계산한다.
	void EnsureTerrainChunksCurrent();
	// 선택적 재료 통계를 계산하고 로그로 출력한다.
	void ScanVoxelArea();
	// 진행 중 요청이 없을 때 새 서버 퇴적 요청 하나를 생성한다.
	void RequestDepositArea();
	// 요청의 현재 단계를 Tick 예산만큼 처리하고 변경 델타를 복제 기록에 추가한다.
	void ProcessServerDepositRequests();
	// 고정 메시용 비동기 트레이스 스캔을 시작하고, 매 Tick 결과 회수와 새 트레이스 발행을 진행한다.
	bool BeginStaticMeshSurfaceScan(const FDRVoxelDepositInBoxSettings& RequestSettings);
	void ProcessStaticMeshSurfaceScan();
	// 유효 히트에 낮은 표면 선호 확률을 적용한 뒤 현재 복셀 요청에 후보로 주입한다.
	void FinishStaticMeshSurfaceScan();
	// 기능 비활성화, 월드 교체, 액터 종료 시 아직 완료되지 않은 핸들과 임시 결과를 폐기한다.
	void CancelStaticMeshSurfaceScan();
	// 클라이언트에서 퇴적/굴착 배열을 공통 Revision 순서로 병합해 재생한다.
	void ApplyPendingDeltaRecords();
	// 설정된 Revision 보존 범위보다 오래된 두 종류의 기록을 함께 제거한다.
	void TrimReplicatedTerrainRecords();
	// 서버 TerrainSubsystem의 굴착 완료 이벤트를 구독/해제한다.
	void BindTerrainDugDelegate();
	void UnbindTerrainDugDelegate();
	// 관리 박스와 겹친 굴착을 공통 Revision 기록으로 변환한다.
	void HandleTerrainDug(const FVector& Location, float Radius);
	// 아래 함수들은 게임 데이터에 영향을 주지 않는 에디터/런타임 디버그 표시다.
	void DrawScanDebugBox() const;
	void DrawDepositGridPoints() const;
	void DrawTerrainChunkBoxes() const;
};
