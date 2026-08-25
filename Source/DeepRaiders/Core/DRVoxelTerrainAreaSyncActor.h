#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelTerrainQueryLibrary.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "WorldCollision.h"
#include "DRVoxelTerrainAreaSyncActor.generated.h"

class AVoxelWorld;
class UDRVoxelTerrainSubsystem;
class ADRVoxelTerrainAreaSyncActor;

UENUM(BlueprintType)
enum class EDRDepositPerformancePreset : uint8
{
	// 프레임 부하를 가장 낮게 유지하고 전체 패스 완료 시간을 길게 잡는다.
	Low,
	// 일반적인 서버 플레이를 위한 기본 균형값이다.
	Balanced,
	// 여유 있는 서버에서 청크 처리 시간을 단축한다.
	High
};

// 관리 박스를 XY로 나눈 표면 처리 청크다. 표면 탐색은 각 XY 영역에서 전체 Z 범위를 한 번 내려가므로
// 기존 3D 청크처럼 같은 XY 열을 Z 청크마다 반복해서 조사하지 않는다.
USTRUCT(BlueprintType)
struct FDRVoxelTerrainChunkBounds
{
	GENERATED_BODY()

	// 관리 박스의 좌하단을 원점으로 한 XY 청크 번호다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FIntPoint ChunkCoordinate = FIntPoint::ZeroValue;

	// 가장자리 청크는 ChunkWorldSize보다 작을 수 있으므로 실제 월드 중심과 반크기를 저장한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FVector BoxCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FVector BoxExtent = FVector::ZeroVector;

	// 이 청크가 마지막으로 완료된 전체 퇴적 패스 번호다. 디버그 및 진행 상태 확인용이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	int32 LastProcessedPass = INDEX_NONE;
};

// 커버리지 선택을 통과한 고정 메시 중심의 원형 범위를 실제 메시 형상에 다시 투영할 한 셀이다.
// 리플렉션이나 복제 대상이 아닌 서버 런타임 자료이므로 가벼운 일반 C++ 구조체로 유지한다.
struct FDRStaticMeshFootprintTraceTarget
{
	FIntPoint VoxelXY = FIntPoint::ZeroValue;
	float AmountScale = 1.f;
	float MinLocalSurfaceZ = 0.f;
	float MaxLocalSurfaceZ = 0.f;
};

// 비동기 핸들과 그 핸들이 어떤 풋프린트 셀을 검사하는지 함께 보관한다.
// 완료 순서가 발행 순서와 달라도 결과를 올바른 X/Y 셀과 강도에 연결할 수 있다.
struct FDRPendingStaticMeshFootprintTrace
{
	FTraceHandle Handle;
	FDRStaticMeshFootprintTraceTarget Target;
};

UENUM()
enum class EDRTerrainEditType : uint8
{
	Deposit,
	Dig
};

// FastArray의 단일 편집 항목이다. 퇴적은 고정 32^3 복셀 청크 하나를, 굴착은 구 중심과 반지름을 저장한다.
// 커스텀 NetSerialize가 퇴적 값을 16비트로, 정렬된 LocalIndex 차이를 packed integer로 전송한다.
USTRUCT()
struct FDRTerrainEditFastArrayItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	EDRTerrainEditType Type = EDRTerrainEditType::Deposit;

	UPROPERTY()
	int32 Revision = 0;

	UPROPERTY()
	FIntVector ChunkCoordinate = FIntVector::ZeroValue;

	UPROPERTY()
	uint8 MaterialIndex = 0;

	UPROPERTY()
	TArray<FDRVoxelCompressedValueDelta> Deltas;

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY()
	float Radius = 0.f;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FDRTerrainEditFastArrayItem>
	: public TStructOpsTypeTraitsBase2<FDRTerrainEditFastArrayItem>
{
	enum
	{
		WithNetSerializer = true
	};
};

// 퇴적과 굴착을 하나의 Revision 정렬 배열로 복제한다. 새 항목과 제거 항목만 FastArray delta로 전송된다.
USTRUCT()
struct FDRTerrainEditFastArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDRTerrainEditFastArrayItem> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FastArrayDeltaSerialize<
			FDRTerrainEditFastArrayItem,
			FDRTerrainEditFastArray>(Items, DeltaParams, *this);
	}

	void SetOwner(ADRVoxelTerrainAreaSyncActor* InOwner)
	{
		Owner = InOwner;
	}

	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

private:
	void NotifyOwner();
	ADRVoxelTerrainAreaSyncActor* Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FDRTerrainEditFastArray>
	: public TStructOpsTypeTraitsBase2<FDRTerrainEditFastArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

// 서버에서 여러 틱의 퇴적 변경을 합치는 동안 같은 복셀의 마지막 값과 머터리얼만 보관한다.
struct FDRPendingDepositVoxelValue
{
	int32 QuantizedValue = 0;
	uint8 MaterialIndex = 0;
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

	// RebuildTerrainChunks가 만든 XY 표면 청크 경계를 월드 공간 박스로 표시한다.
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

	// 1차 저해상도 스캔과 커버리지 선택이 끝난 뒤, 실제로 선택된 고정 메시 패치만 복셀 셀 단위로
	// 다시 추적한다. 이 단계에서 메시 바깥 셀과 급경사 셀을 제거해 떠 있는 수평 눈판을 방지한다.
	TArray<FDRPendingStaticMeshFootprintTrace> PendingStaticMeshFootprintTraces;
	TArray<FDRStaticMeshFootprintTraceTarget> StaticMeshFootprintTraceTargets;
	FVector StaticMeshFootprintScanCenter = FVector::ZeroVector;
	FVector StaticMeshFootprintScanExtent = FVector::ZeroVector;
	int32 NextStaticMeshFootprintTraceIndex = 0;
	bool bStaticMeshFootprintScanActive = false;

	// 한 번의 전체 패스에서 모든 청크를 정확히 한 번씩 처리한다. 순서를 Seed로 섞어 맵 한쪽부터
	// 누적되는 모습을 막고, 진행 중 패스가 끝나기 전에는 타이머가 새 패스를 중첩하지 않는다.
	TArray<int32> DepositChunkOrder;
	int32 NextDepositChunkOrderIndex = 0;
	int32 ActiveDepositChunkIndex = INDEX_NONE;
	int32 DepositPassNumber = 0;
	bool bDepositPassActive = false;
	// 서로 다른 청크의 확장 풋프린트가 같은 복셀에서 겹쳐도 한 패스에서는 한 번만 기록하도록 공유한다.
	// 패스가 끝나거나 취소되면 비워지므로 장기 동기화 히스토리와는 무관한 서버 런타임 상태다.
	TSet<FIntVector> DepositPassWrittenVoxelPositions;
	TSet<FIntPoint> DepositPassWrittenColumns;

	// 여러 틱에서 같은 복셀을 다시 변경하면 마지막 값만 남긴 뒤 0.1초 또는 청크 완료 시 FastArray로 확정한다.
	TMap<FIntVector, FDRPendingDepositVoxelValue> PendingDepositVoxelValues;
	float PendingDepositBatchAge = 0.f;

	// 클라이언트가 마지막으로 성공적으로 적용한 Revision과 중복 경고 방지용 누락 Revision이다.
	int32 LastAppliedTerrainRevision = 0;
	int32 LastReportedMissingRevision = 0;

	// 서버 타이머 핸들과 굴착 델리게이트 생명주기를 보관한다.
	FTimerHandle ScanTimerHandle;
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
	// 선택적 재료 통계를 계산하고 로그로 출력한다.
	void ScanVoxelArea();
	// 진행 중 패스가 없을 때 전체 청크 순환을 시작한다.
	void RequestDepositArea();
	// 현재 패스의 다음 청크 요청을 생성한다. 요청/메시 스캔이 남아 있으면 아무 작업도 하지 않는다.
	void StartNextDepositChunk();
	// 기능 비활성화, 월드 교체, 청크 레이아웃 변경 시 현재 패스와 요청 상태를 함께 폐기한다.
	void CancelDepositPass();
	// 요청의 현재 단계를 Tick 예산만큼 처리하고 변경 델타를 복제 기록에 추가한다.
	void ProcessServerDepositRequests();
	// 현재 월드와 일치하는 유효한 첫 요청을 반환한다. 단계 지정 버전은 상태 머신 진입 조건도 확인한다.
	FDRVoxelDepositInBoxRequest* GetActiveDepositRequest();
	FDRVoxelDepositInBoxRequest* GetActiveDepositRequest(EDRVoxelDepositRequestPhase ExpectedPhase);
	// 라이브러리의 틱별 임시 레코드를 절대 복셀 좌표로 풀어 대기 배치에 병합한다.
	void AccumulateDepositDeltaRecord(const FDRVoxelDepositDeltaRecord& DeltaRecord);
	// 대기 변경을 고정 32^3 청크/머터리얼별 FastArray 항목으로 확정한다.
	void FlushPendingDepositBatch();
	// 새 통합 편집 항목을 추가하고 FastArray에 dirty 표시한다.
	void AddTerrainEditItem(FDRTerrainEditFastArrayItem&& Item);
	// 고정 메시용 비동기 트레이스 스캔을 시작하고, 매 Tick 결과 회수와 새 트레이스 발행을 진행한다.
	bool BeginStaticMeshSurfaceScan(
		const FDRVoxelDepositInBoxSettings& RequestSettings,
		const FVector& ScanCenter,
		const FVector& ScanExtent);
	void ProcessStaticMeshSurfaceScan();
	// 유효 히트를 정렬해 현재 복셀 요청에 주입한다. 최종 퍼센트/낮은 지형 선택은 라이브러리가 통합 수행한다.
	void FinishStaticMeshSurfaceScan();
	// 기능 비활성화, 월드 교체, 액터 종료 시 아직 완료되지 않은 핸들과 임시 결과를 폐기한다.
	void CancelStaticMeshSurfaceScan();
	// 선택된 메시 중심의 원형 풋프린트를 셀별 비동기 트레이스로 정밀하게 표면에 투영한다.
	bool BeginStaticMeshFootprintScan();
	void ProcessStaticMeshFootprintScan();
	// 정밀 결과가 모두 준비됐음을 요청에 알린 뒤 라이브러리의 Apply 단계가 진행되게 한다.
	void FinishStaticMeshFootprintScan();
	// 진행 중 정밀 핸들과 임시 대상만 폐기한다. 요청을 끝낼지는 호출부가 별도로 결정한다.
	void CancelStaticMeshFootprintScan();
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
	void DrawScanDebugBox() const;
	void DrawDepositGridPoints() const;
	void DrawTerrainChunkBoxes() const;
};
