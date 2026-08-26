#include "DRVoxelTerrainSyncLibrary.h"

#include "VoxelWorld.h"

namespace
{
	int32 FloorDivide(int32 Value, int32 Divisor)
	{
		check(Divisor > 0);
		// -1 - ((-1 - Value) / Divisor)는 MIN_int32도 넘치지 않으면서
		// 음수의 정확한 배수(-32 등)를 이전 청크로 잘못 내리지 않는다.
		return Value >= 0
			? Value / Divisor
			: -1 - ((-1 - Value) / Divisor);
	}

	FIntVector GetNetworkChunkCoordinate(const FIntVector& Position)
	{
		// C++ 정수 나눗셈은 0 방향으로 버리므로 음수 축에서는 FloorDivide를 반드시 사용한다.
		// 예: X=-1과 X=-32는 청크 -1, X=-33은 청크 -2에 속한다.
		return FIntVector(
			FloorDivide(Position.X, DRVoxelTerrainSync::NetworkChunkSize),
			FloorDivide(Position.Y, DRVoxelTerrainSync::NetworkChunkSize),
			FloorDivide(Position.Z, DRVoxelTerrainSync::NetworkChunkSize));
	}

	FIntVector GetNetworkChunkMin(const FIntVector& ChunkCoordinate)
	{
		return ChunkCoordinate * DRVoxelTerrainSync::NetworkChunkSize;
	}

	int32 GetNetworkChunkLocalIndex(
		const FIntVector& Position,
		const FIntVector& ChunkCoordinate)
	{
		const FIntVector Local = Position - GetNetworkChunkMin(ChunkCoordinate);
		checkf(
			Local.X >= 0 && Local.X < DRVoxelTerrainSync::NetworkChunkSize &&
			Local.Y >= 0 && Local.Y < DRVoxelTerrainSync::NetworkChunkSize &&
			Local.Z >= 0 && Local.Z < DRVoxelTerrainSync::NetworkChunkSize,
			TEXT("Invalid network chunk local coordinate. Position=%s Chunk=%s Local=%s"),
			*Position.ToString(),
			*ChunkCoordinate.ToString(),
			*Local.ToString());
		// X가 가장 빠르고 Z가 가장 느리게 증가하는 TerrainOperationLibrary의 선형 인덱스 규칙과 동일하다.
		return Local.X + Local.Y * DRVoxelTerrainSync::NetworkChunkSize +
			Local.Z * DRVoxelTerrainSync::NetworkChunkSize * DRVoxelTerrainSync::NetworkChunkSize;
	}

	struct FDRNetworkDepositGroupKey
	{
		// 같은 네트워크 청크라도 머터리얼이 다르면 별도 FastArray 항목이어야 한다.
		// 한 항목에는 MaterialIndex가 하나만 존재하기 때문이다.
		FIntVector ChunkCoordinate = FIntVector::ZeroValue;
		uint8 MaterialIndex = 0;

		bool operator==(const FDRNetworkDepositGroupKey& Other) const
		{
			return ChunkCoordinate == Other.ChunkCoordinate &&
				MaterialIndex == Other.MaterialIndex;
		}
	};

	uint32 GetTypeHash(const FDRNetworkDepositGroupKey& Key)
	{
		uint32 Hash = HashCombine(
			::GetTypeHash(Key.ChunkCoordinate.X),
			::GetTypeHash(Key.ChunkCoordinate.Y));
		Hash = HashCombine(Hash, ::GetTypeHash(Key.ChunkCoordinate.Z));
		return HashCombine(Hash, ::GetTypeHash(Key.MaterialIndex));
	}

	bool IsGroupKeyBefore(
		const FDRNetworkDepositGroupKey& A,
		const FDRNetworkDepositGroupKey& B)
	{
		// TMap의 순회 순서는 비결정적이므로 청크 XYZ와 머터리얼 순으로 고정한다.
		// 이 정렬은 네트워크 결과와 자동화 테스트의 재현성을 높인다.
		if (A.ChunkCoordinate.X != B.ChunkCoordinate.X)
		{
			return A.ChunkCoordinate.X < B.ChunkCoordinate.X;
		}
		if (A.ChunkCoordinate.Y != B.ChunkCoordinate.Y)
		{
			return A.ChunkCoordinate.Y < B.ChunkCoordinate.Y;
		}
		if (A.ChunkCoordinate.Z != B.ChunkCoordinate.Z)
		{
			return A.ChunkCoordinate.Z < B.ChunkCoordinate.Z;
		}
		return A.MaterialIndex < B.MaterialIndex;
	}

	bool ApplyEditItem(AVoxelWorld* VoxelWorld, const FDRTerrainEditFastArrayItem& Item)
	{
		// 복제 형식은 네트워크 청크 좌표를 저장하지만 지형 조작 API는 VoxelMin/Max를 요구한다.
		// 이 함수가 wire format을 런타임 델타 형식으로 복원한 뒤 기존 월드 편집 경로를 재사용한다.
		if (Item.Type == EDRTerrainEditType::Deposit)
		{
			FDRVoxelDepositDeltaRecord DepositRecord;
			DepositRecord.Revision = Item.Revision;
			DepositRecord.VoxelMin = GetNetworkChunkMin(Item.ChunkCoordinate);
			DepositRecord.VoxelMax = DepositRecord.VoxelMin +
				FIntVector(DRVoxelTerrainSync::NetworkChunkSize - 1);
			DepositRecord.MaterialIndex = Item.MaterialIndex;
			DepositRecord.Deltas = Item.Deltas;

			int32 AppliedVoxelCount = 0;
			return UDRVoxelTerrainOperationLibrary::ApplyDepositDeltaRecord(
				VoxelWorld,
				DepositRecord,
				AppliedVoxelCount);
		}

		FDRVoxelDigDeltaRecord DigRecord;
		DigRecord.Revision = Item.Revision;
		DigRecord.Location = Item.Location;
		DigRecord.Radius = Item.Radius;
		return UDRVoxelTerrainOperationLibrary::ApplyDigDeltaRecord(VoxelWorld, DigRecord);
	}

	void SetMissingRevisionIssue(
		FDRTerrainSyncReplayResult& Result,
		FDRTerrainSyncClientState& State,
		EDRTerrainSyncReplayIssue Issue,
		int32 ExpectedRevision,
		int32 AvailableRevision)
	{
		Result.Issue = Issue;
		Result.ExpectedRevision = ExpectedRevision;
		Result.AvailableRevision = AvailableRevision;
		// 같은 누락은 Tick마다 반복될 수 있으므로 최초 발견 때만 로그하도록 상태에 기록한다.
		// 다른 Revision을 기다리게 되면 값이 달라져 다시 한 번 보고할 수 있다.
		Result.bShouldReport =
			State.LastReportedMissingRevision != ExpectedRevision;
		State.LastReportedMissingRevision = ExpectedRevision;
	}
}

void FDRVoxelTerrainSyncLibrary::AccumulateDepositDelta(
	const FDRVoxelDepositDeltaRecord& DeltaRecord,
	FDRTerrainSyncServerState& State)
{
	// Delta.LocalIndex를 절대 좌표로 풀려면 원본 요청 박스의 포괄적 크기가 필요하다.
	// 비어 있거나 overflow/역전된 박스는 네트워크 상태에 섞지 않고 무시한다.
	DRVoxelTerrain::FInclusiveVoxelBoxDimensions Dimensions;
	if (DeltaRecord.Deltas.Num() == 0 ||
		!DRVoxelTerrain::TryGetInclusiveVoxelBoxDimensions(
			DeltaRecord.VoxelMin,
			DeltaRecord.VoxelMax,
			Dimensions))
	{
		return;
	}

	for (const FDRVoxelCompressedValueDelta& Delta : DeltaRecord.Deltas)
	{
		if (Delta.LocalIndex < 0 || Delta.LocalIndex >= Dimensions.TotalVoxelCount)
		{
			continue;
		}

		const FIntVector Position = DRVoxelTerrain::GetInclusiveVoxelPosition(
			Delta.LocalIndex,
			DeltaRecord.VoxelMin,
			Dimensions);
		// FindOrAdd 후 항상 값을 대입하므로 같은 복셀이 여러 Tick에 수정되면 마지막 상태가 남는다.
		// 중간 상태를 전송하지 않아도 클라이언트는 배치 경계의 서버 최종 상태와 일치한다.
		FDRPendingDepositVoxelValue& PendingValue =
			State.PendingDepositVoxelValues.FindOrAdd(Position);
		PendingValue.QuantizedValue = FMath::Clamp(
			Delta.QuantizedValue,
			-DRVoxelTerrain::QuantizedValueMax,
			DRVoxelTerrain::QuantizedValueMax);
		PendingValue.MaterialIndex = DeltaRecord.MaterialIndex;
	}
}

bool FDRVoxelTerrainSyncLibrary::AdvanceDepositBatchTimer(
	FDRTerrainSyncServerState& State,
	float DeltaSeconds,
	float BatchIntervalSeconds)
{
	if (State.PendingDepositVoxelValues.Num() == 0)
	{
		// 빈 상태에서 시간을 누적하면 다음 첫 변경이 즉시 전송될 수 있으므로 반드시 0으로 유지한다.
		State.PendingDepositBatchAge = 0.f;
		return false;
	}

	// 음수 DeltaSeconds는 시간 역행으로 취급하지 않고 0으로 제한한다.
	State.PendingDepositBatchAge += FMath::Max(0.f, DeltaSeconds);
	return State.PendingDepositBatchAge >= FMath::Max(0.f, BatchIntervalSeconds);
}

int32 FDRVoxelTerrainSyncLibrary::ConsumeDepositBatch(
	FDRTerrainSyncServerState& State,
	TArray<FDRTerrainEditFastArrayItem>& OutItems)
{
	OutItems.Reset();
	const int32 BatchedVoxelCount = State.PendingDepositVoxelValues.Num();
	if (BatchedVoxelCount == 0)
	{
		State.PendingDepositBatchAge = 0.f;
		return 0;
	}

	// 1단계: 절대 좌표 맵을 (32^3 청크, 머터리얼)별 델타 배열로 재그룹한다.
	TMap<FDRNetworkDepositGroupKey, TArray<FDRVoxelCompressedValueDelta>> GroupedDeltas;
	GroupedDeltas.Reserve(BatchedVoxelCount);
	for (const TPair<FIntVector, FDRPendingDepositVoxelValue>& PendingPair
		: State.PendingDepositVoxelValues)
	{
		FDRNetworkDepositGroupKey GroupKey;
		GroupKey.ChunkCoordinate = GetNetworkChunkCoordinate(PendingPair.Key);
		GroupKey.MaterialIndex = PendingPair.Value.MaterialIndex;

		FDRVoxelCompressedValueDelta Delta;
		Delta.LocalIndex = GetNetworkChunkLocalIndex(PendingPair.Key, GroupKey.ChunkCoordinate);
		Delta.QuantizedValue = PendingPair.Value.QuantizedValue;
		GroupedDeltas.FindOrAdd(GroupKey).Add(Delta);
	}

	// 2단계: TMap의 비결정적 순서를 제거해 생성되는 Revision 순서가 실행마다 달라지지 않게 한다.
	TArray<FDRNetworkDepositGroupKey> SortedGroupKeys;
	GroupedDeltas.GenerateKeyArray(SortedGroupKeys);
	SortedGroupKeys.Sort(IsGroupKeyBefore);

	// 이 시점부터 OutItems가 배치의 유일한 소유자다. Actor는 각 항목에 Revision을 붙여 FastArray에 넣는다.
	State.PendingDepositVoxelValues.Reset();
	State.PendingDepositBatchAge = 0.f;
	OutItems.Reserve(SortedGroupKeys.Num());
	for (const FDRNetworkDepositGroupKey& GroupKey : SortedGroupKeys)
	{
		TArray<FDRVoxelCompressedValueDelta>& Deltas = GroupedDeltas.FindChecked(GroupKey);
		// NetSerialize는 이전 LocalIndex와의 차이를 packed integer로 보내므로 엄격한 오름차순이 필수다.
		Deltas.Sort([](
			const FDRVoxelCompressedValueDelta& A,
			const FDRVoxelCompressedValueDelta& B)
		{
			return A.LocalIndex < B.LocalIndex;
		});

		FDRTerrainEditFastArrayItem& Item = OutItems.AddDefaulted_GetRef();
		Item.Type = EDRTerrainEditType::Deposit;
		Item.ChunkCoordinate = GroupKey.ChunkCoordinate;
		Item.MaterialIndex = GroupKey.MaterialIndex;
		Item.Deltas = MoveTemp(Deltas);
	}

	return BatchedVoxelCount;
}

bool FDRVoxelTerrainSyncLibrary::HasPendingEdits(
	const TArray<FDRTerrainEditFastArrayItem>& Items,
	int32 ServerRevision,
	const FDRTerrainSyncClientState& State)
{
	// 일반 프로퍼티인 ServerRevision과 FastArray payload의 복제 도착 순서는 서로 다를 수 있다.
	// 둘 중 하나라도 클라이언트 커서보다 앞서면 ReplayAvailableEdits를 다시 시도한다.
	return ServerRevision > State.LastAppliedRevision ||
		(Items.Num() > 0 && Items.Last().Revision > State.LastAppliedRevision);
}

int32 FDRVoxelTerrainSyncLibrary::CalculateTrimCount(
	const TArray<FDRTerrainEditFastArrayItem>& Items,
	int32 FinalRevision,
	int32 MaxRecordCount)
{
	if (MaxRecordCount <= 0)
	{
		return 0;
	}

	// 최종 Revision을 기준으로 보존할 가장 오래된 번호를 구하고, 그보다 작은 앞부분만 제거한다.
	// Items가 연속적이지 않더라도 upper_bound 결과는 현재 배열에서 안전하게 제거 가능한 prefix 길이다.
	const int32 MinimumRevisionToKeep = FMath::Max(
		1,
		FinalRevision - MaxRecordCount + 1);
	return FindFirstRecordAfterRevision(Items, MinimumRevisionToKeep - 1);
}

FDRTerrainSyncReplayResult FDRVoxelTerrainSyncLibrary::ReplayAvailableEdits(
	AVoxelWorld* VoxelWorld,
	const TArray<FDRTerrainEditFastArrayItem>& Items,
	int32 ServerRevision,
	FDRTerrainSyncClientState& State)
{
	FDRTerrainSyncReplayResult Result;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return Result;
	}

	// 이미 적용한 prefix를 선형으로 다시 걷지 않고 upper_bound에서 시작한다.
	int32 EditIndex = FindFirstRecordAfterRevision(Items, State.LastAppliedRevision);
	while (EditIndex < Items.Num())
	{
		const FDRTerrainEditFastArrayItem& Item = Items[EditIndex];
		const int32 ExpectedRevision = State.LastAppliedRevision + 1;
		if (Item.Revision != ExpectedRevision)
		{
			// 뒤 Revision을 먼저 적용하면 굴착과 퇴적의 서버 순서가 뒤집힌다.
			// 누락 항목이 FastArray로 도착할 때까지 이후 모든 편집을 보류한다.
			SetMissingRevisionIssue(
				Result,
				State,
				EDRTerrainSyncReplayIssue::RevisionGap,
				ExpectedRevision,
				Item.Revision);
			return Result;
		}

		if (!ApplyEditItem(VoxelWorld, Item))
		{
			// 월드 생성 상태나 실제 편집 실패 시에도 커서를 유지해 다음 호출에서 같은 항목을 재시도한다.
			return Result;
		}

		// 월드 적용이 성공한 뒤에만 커서를 전진시킨다. 성공한 새 Revision은 이전 누락 경고도 해제한다.
		State.LastAppliedRevision = Item.Revision;
		State.LastReportedMissingRevision = 0;
		++EditIndex;
	}

	const int32 ExpectedRevision = State.LastAppliedRevision + 1;
	if (ServerRevision >= ExpectedRevision)
	{
		// 최종 번호는 도착했지만 payload가 아직 없거나 서버에서 이미 trim된 경우다.
		// FastArray 지연이면 이후 콜백에서 회복되고, 영구 trim이면 Actor 로그가 체크포인트 필요성을 알린다.
		SetMissingRevisionIssue(
			Result,
			State,
			EDRTerrainSyncReplayIssue::RecordUnavailable,
			ExpectedRevision,
			ServerRevision);
	}

	return Result;
}

int32 FDRVoxelTerrainSyncLibrary::FindFirstRecordAfterRevision(
	const TArray<FDRTerrainEditFastArrayItem>& Items,
	int32 Revision)
{
	// std::upper_bound와 같은 의미다: Item.Revision > Revision인 첫 인덱스를 O(log N)에 찾는다.
	int32 MinIndex = 0;
	int32 MaxIndex = Items.Num();
	while (MinIndex < MaxIndex)
	{
		const int32 MiddleIndex = MinIndex + (MaxIndex - MinIndex) / 2;
		if (Items[MiddleIndex].Revision <= Revision)
		{
			MinIndex = MiddleIndex + 1;
		}
		else
		{
			MaxIndex = MiddleIndex;
		}
	}

	return MinIndex;
}
