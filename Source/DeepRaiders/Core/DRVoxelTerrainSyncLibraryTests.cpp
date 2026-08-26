#include "DRVoxelTerrainSyncLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

// 네트워크 청크 경계의 양쪽과 음수 좌표를 함께 넣어 floor division과 LocalIndex 변환을 검증한다.
// 특히 C++의 0 방향 정수 나눗셈을 그대로 사용하면 -1/-33 위치가 잘못된 청크로 갈 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDRVoxelTerrainSyncBatchBoundaryTest,
	"DeepRaiders.VoxelTerrain.Sync.BatchChunkBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRVoxelTerrainSyncBatchBoundaryTest::RunTest(const FString& Parameters)
{
	FDRVoxelDepositDeltaRecord Record;
	Record.VoxelMin = FIntVector(-33, 0, 0);
	Record.VoxelMax = FIntVector(32, 0, 0);
	Record.MaterialIndex = 7;
	for (const int32 X : {-33, -32, -1, 0, 31, 32})
	{
		FDRVoxelCompressedValueDelta& Delta = Record.Deltas.AddDefaulted_GetRef();
		Delta.LocalIndex = X - Record.VoxelMin.X;
		Delta.QuantizedValue = X;
	}

	FDRTerrainSyncServerState State;
	State.PendingDepositBatchAge = 1.f;
	FDRVoxelTerrainSyncLibrary::AccumulateDepositDelta(Record, State);

	TArray<FDRTerrainEditFastArrayItem> Items;
	const int32 BatchedVoxelCount =
		FDRVoxelTerrainSyncLibrary::ConsumeDepositBatch(State, Items);

	TestEqual(TEXT("All boundary voxels are batched"), BatchedVoxelCount, 6);
	TestEqual(TEXT("Boundary voxels span four network chunks"), Items.Num(), 4);
	TestEqual(TEXT("Pending map is consumed"), State.PendingDepositVoxelValues.Num(), 0);
	TestEqual(TEXT("Batch age is reset"), State.PendingDepositBatchAge, 0.f);

	const TArray<FIntVector> ExpectedChunks = {
		FIntVector(-2, 0, 0),
		FIntVector(-1, 0, 0),
		FIntVector(0, 0, 0),
		FIntVector(1, 0, 0)};
	const TArray<int32> ExpectedDeltaCounts = {1, 2, 2, 1};
	for (int32 Index = 0; Index < ExpectedChunks.Num(); ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("Chunk %d is sorted and preserves negative floor division"), Index),
			Items.IsValidIndex(Index) && Items[Index].ChunkCoordinate == ExpectedChunks[Index]);
		TestTrue(
			FString::Printf(TEXT("Chunk %d has the expected voxel count"), Index),
			Items.IsValidIndex(Index) && Items[Index].Deltas.Num() == ExpectedDeltaCounts[Index]);
	}

	TestTrue(TEXT("-33 maps to local X 31"), Items[0].Deltas[0].LocalIndex == 31);
	TestTrue(TEXT("-32 and -1 map to local X 0 and 31"),
		Items[1].Deltas[0].LocalIndex == 0 && Items[1].Deltas[1].LocalIndex == 31);
	TestTrue(TEXT("0 and 31 map to local X 0 and 31"),
		Items[2].Deltas[0].LocalIndex == 0 && Items[2].Deltas[1].LocalIndex == 31);
	TestTrue(TEXT("32 maps to local X 0"), Items[3].Deltas[0].LocalIndex == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDRVoxelTerrainSyncBatchLatestValueTest,
	"DeepRaiders.VoxelTerrain.Sync.BatchKeepsLatestValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRVoxelTerrainSyncBatchLatestValueTest::RunTest(const FString& Parameters)
{
	// 한 배치 안에서 같은 절대 복셀이 반복 변경되면 중간 상태가 아니라
	// 마지막 값과 마지막 머터리얼 하나만 전송되는 last-write-wins 계약을 확인한다.
	FDRVoxelDepositDeltaRecord FirstRecord;
	FirstRecord.VoxelMin = FIntVector::ZeroValue;
	FirstRecord.VoxelMax = FIntVector::ZeroValue;
	FirstRecord.MaterialIndex = 1;
	FirstRecord.Deltas.Add({0, 100});

	FDRVoxelDepositDeltaRecord LastRecord = FirstRecord;
	LastRecord.MaterialIndex = 9;
	LastRecord.Deltas[0].QuantizedValue = 200;

	FDRTerrainSyncServerState State;
	FDRVoxelTerrainSyncLibrary::AccumulateDepositDelta(FirstRecord, State);
	FDRVoxelTerrainSyncLibrary::AccumulateDepositDelta(LastRecord, State);

	TArray<FDRTerrainEditFastArrayItem> Items;
	const int32 BatchedVoxelCount =
		FDRVoxelTerrainSyncLibrary::ConsumeDepositBatch(State, Items);
	TestEqual(TEXT("Repeated voxel is emitted once"), BatchedVoxelCount, 1);
	TestEqual(TEXT("Repeated voxel produces one record"), Items.Num(), 1);
	TestTrue(TEXT("Latest material is retained"), Items.Num() == 1 && Items[0].MaterialIndex == 9);
	TestTrue(TEXT("Latest value is retained"),
		Items.Num() == 1 && Items[0].Deltas.Num() == 1 && Items[0].Deltas[0].QuantizedValue == 200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDRVoxelTerrainSyncRevisionHelpersTest,
	"DeepRaiders.VoxelTerrain.Sync.RevisionHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRVoxelTerrainSyncRevisionHelpersTest::RunTest(const FString& Parameters)
{
	// 실제 VoxelWorld 없이 검증 가능한 순수 상태 규칙을 묶는다:
	// 새 Revision 감지, 보존 개수 계산, 빈 배치 타이머 정지, 주기 도달 판정.
	TArray<FDRTerrainEditFastArrayItem> Items;
	for (int32 Revision = 1; Revision <= 5; ++Revision)
	{
		FDRTerrainEditFastArrayItem& Item = Items.AddDefaulted_GetRef();
		Item.Revision = Revision;
	}

	FDRTerrainSyncClientState ClientState;
	ClientState.LastAppliedRevision = 3;
	TestTrue(TEXT("A later revision is detected"),
		FDRVoxelTerrainSyncLibrary::HasPendingEdits(Items, 5, ClientState));
	ClientState.LastAppliedRevision = 5;
	TestFalse(TEXT("No edit is pending after the final revision"),
		FDRVoxelTerrainSyncLibrary::HasPendingEdits(Items, 5, ClientState));
	TestEqual(TEXT("Disabled trimming keeps every record"),
		FDRVoxelTerrainSyncLibrary::CalculateTrimCount(Items, 5, 0), 0);
	TestEqual(TEXT("Keeping three revisions trims the first two"),
		FDRVoxelTerrainSyncLibrary::CalculateTrimCount(Items, 5, 3), 2);

	FDRTerrainSyncServerState ServerState;
	TestFalse(TEXT("Empty batch timer does not advance"),
		FDRVoxelTerrainSyncLibrary::AdvanceDepositBatchTimer(ServerState, 1.f, 0.1f));
	ServerState.PendingDepositVoxelValues.Add(
		FIntVector::ZeroValue,
		FDRPendingDepositVoxelValue{});
	TestFalse(TEXT("Batch is not ready before the interval"),
		FDRVoxelTerrainSyncLibrary::AdvanceDepositBatchTimer(ServerState, 0.04f, 0.1f));
	TestTrue(TEXT("Batch becomes ready after the interval"),
		FDRVoxelTerrainSyncLibrary::AdvanceDepositBatchTimer(ServerState, 0.07f, 0.1f));
	return true;
}

#endif
