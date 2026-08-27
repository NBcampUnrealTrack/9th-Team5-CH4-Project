#include "DRVoxelTerrainOperationLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDRVoxelTerrainRandomSampleIndicesTest,
	"DeepRaiders.VoxelTerrain.Operation.RandomSampleIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRVoxelTerrainRandomSampleIndicesTest::RunTest(const FString& Parameters)
{
	FRandomStream FirstStream(12345);
	TArray<int32> FirstIndices;
	DRVoxelTerrain::BuildRandomUniqueIndices(1000, 16, FirstStream, FirstIndices);

	TestEqual(TEXT("Requested sample count is respected"), FirstIndices.Num(), 16);
	TSet<int32> UniqueIndices;
	for (const int32 Index : FirstIndices)
	{
		UniqueIndices.Add(Index);
	}
	TestEqual(TEXT("Random samples contain no duplicates"), UniqueIndices.Num(), FirstIndices.Num());
	for (const int32 Index : FirstIndices)
	{
		TestTrue(TEXT("Random sample stays inside the population"), Index >= 0 && Index < 1000);
	}

	FRandomStream RepeatStream(12345);
	TArray<int32> RepeatIndices;
	DRVoxelTerrain::BuildRandomUniqueIndices(1000, 16, RepeatStream, RepeatIndices);
	TestTrue(TEXT("The same seed reproduces the same sample order"), FirstIndices == RepeatIndices);

	FRandomStream SmallPopulationStream(7);
	TArray<int32> SmallPopulationIndices;
	DRVoxelTerrain::BuildRandomUniqueIndices(5, 20, SmallPopulationStream, SmallPopulationIndices);
	TestEqual(TEXT("Sample count is capped by the population"), SmallPopulationIndices.Num(), 5);
	TSet<int32> SmallPopulationUniqueIndices;
	for (const int32 Index : SmallPopulationIndices)
	{
		SmallPopulationUniqueIndices.Add(Index);
	}
	TestEqual(
		TEXT("A capped sample still contains every population index once"),
		SmallPopulationUniqueIndices.Num(),
		5);
	return true;
}

#endif
