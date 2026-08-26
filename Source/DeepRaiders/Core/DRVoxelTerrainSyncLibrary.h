#pragma once

#include "CoreMinimal.h"
#include "DRVoxelTerrainOperationLibrary.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "DRVoxelTerrainSyncLibrary.generated.h"

class ADRVoxelTerrainAreaSyncActor;
class AVoxelWorld;

namespace DRVoxelTerrainSync
{
	// 복제 레코드는 작업 청크와 독립적인 고정 32^3 단위로 나눠 LocalIndex를 짧게 유지한다.
	inline constexpr int32 NetworkChunkSize = 32;
	inline constexpr int32 NetworkChunkVoxelCount =
		NetworkChunkSize * NetworkChunkSize * NetworkChunkSize;
}

UENUM()
enum class EDRTerrainEditType : uint8
{
	Deposit,
	Dig
};

// 통합 FastArray의 단일 편집 항목이다. Type에 따라 퇴적 청크 또는 구형 굴착 payload만 직렬화한다.
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

// 퇴적과 굴착을 하나의 Revision 오름차순 스트림으로 복제한다.
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
	// 복제하지 않는 로컬 역참조이며 Actor 생성자와 BeginPlay에서 현재 인스턴스로 갱신한다.
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

struct FDRPendingDepositVoxelValue
{
	int32 QuantizedValue = 0;
	uint8 MaterialIndex = 0;
};

struct FDRTerrainSyncServerState
{
	TMap<FIntVector, FDRPendingDepositVoxelValue> PendingDepositVoxelValues;
	float PendingDepositBatchAge = 0.f;
};

struct FDRTerrainSyncClientState
{
	int32 LastAppliedRevision = 0;
	int32 LastReportedMissingRevision = 0;
};

enum class EDRTerrainSyncReplayIssue : uint8
{
	None,
	RevisionGap,
	RecordUnavailable
};

struct FDRTerrainSyncReplayResult
{
	EDRTerrainSyncReplayIssue Issue = EDRTerrainSyncReplayIssue::None;
	int32 ExpectedRevision = 0;
	int32 AvailableRevision = 0;
	bool bShouldReport = false;
};

// 네트워크 생명주기를 소유하지 않고 지형 편집 동기화 데이터만 변환하는 정적 C++ 라이브러리다.
// Actor, Timer, RPC를 직접 보관하지 않으며 모든 가변 상태는 호출자가 State로 명시적으로 전달한다.
// 덕분에 서버 배치 규칙과 클라이언트 Revision 재생 규칙을 월드 Actor의 실행 흐름과 분리해 테스트할 수 있다.
class DEEPRAIDERS_API FDRVoxelTerrainSyncLibrary final
{
public:
	// 지형 조작 라이브러리가 반환한 청크 상대 델타를 절대 복셀 좌표로 풀어 서버 대기 맵에 병합한다.
	// 같은 복셀이 여러 번 들어오면 마지막 값/머터리얼만 남겨 최종 상태만 전송한다.
	static void AccumulateDepositDelta(
		const FDRVoxelDepositDeltaRecord& DeltaRecord,
		FDRTerrainSyncServerState& State);

	// 대기 값이 있을 때만 배치 나이를 증가시키고 전송 주기에 도달했는지 반환한다.
	// 이 함수는 배치를 비우지 않으므로 true를 받은 호출자가 ConsumeDepositBatch를 호출해야 한다.
	static bool AdvanceDepositBatchTimer(
		FDRTerrainSyncServerState& State,
		float DeltaSeconds,
		float BatchIntervalSeconds);

	// 서버 대기 맵을 고정 32^3 네트워크 청크와 머터리얼별 FastArray 항목으로 변환한다.
	// 성공적으로 소비한 뒤 State는 빈 상태가 되며, 반환값은 중복 병합 후 고유 복셀 수다.
	static int32 ConsumeDepositBatch(
		FDRTerrainSyncServerState& State,
		TArray<FDRTerrainEditFastArrayItem>& OutItems);

	// 최종 Revision 프로퍼티와 FastArray 중 어느 쪽이 먼저 도착해도 재생 필요성을 감지한다.
	static bool HasPendingEdits(
		const TArray<FDRTerrainEditFastArrayItem>& Items,
		int32 ServerRevision,
		const FDRTerrainSyncClientState& State);

	// 오름차순 Items 앞부분에서 제거할 항목 수를 계산한다. MaxRecordCount <= 0이면 보존 제한을 끈다.
	// 체크포인트 없이 오래된 항목을 제거하면 늦게 접속한 클라이언트가 복구할 수 있으므로 호출자가 정책을 책임진다.
	static int32 CalculateTrimCount(
		const TArray<FDRTerrainEditFastArrayItem>& Items,
		int32 FinalRevision,
		int32 MaxRecordCount);

	// LastAppliedRevision 다음 항목부터 연속된 편집만 월드에 적용한다.
	// 중간 Revision이 없거나 월드 적용이 실패하면 커서를 전진시키지 않고 즉시 멈춘다.
	static FDRTerrainSyncReplayResult ReplayAvailableEdits(
		AVoxelWorld* VoxelWorld,
		const TArray<FDRTerrainEditFastArrayItem>& Items,
		int32 ServerRevision,
		FDRTerrainSyncClientState& State);

private:
	// 상태 없는 함수 묶음이므로 인스턴스 생성을 금지한다.
	FDRVoxelTerrainSyncLibrary() = delete;

	// Items가 Revision 오름차순이라는 불변식에 기반한 upper_bound 이진 탐색이다.
	static int32 FindFirstRecordAfterRevision(
		const TArray<FDRTerrainEditFastArrayItem>& Items,
		int32 Revision);
};
