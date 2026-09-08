#include "DeepRaiders/Snow/DRSnowNetworkUtils.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDRSnowSnapshotCompressionTest,
	"DeepRaiders.Snow.SnapshotCompression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRSnowSnapshotCompressionTest::RunTest(const FString& Parameters)
{
	// 작은 payload, Oodle 블록 경계, 압축이 잘 안 되는 데이터까지 왕복 검증한다.
	for (const int32 Size : {1, 16, 65535, 65536, 65537, 1024 * 1024})
	{
		for (const bool bRandom : {false, true})
		{
			TArray<uint8> Original;
			Original.SetNumUninitialized(Size);
			FRandomStream Random(12345);
			for (uint8& Byte : Original)
			{
				Byte = bRandom ? static_cast<uint8>(Random.GetUnsignedInt() >> 24) : 42;
			}
			TArray<uint8> Compressed;
			TArray<uint8> Restored;
			if (!TestTrue(TEXT("Oodle compression succeeds"),
				FDRSnowNetworkUtils::CompressSnapshotData(Original, Compressed)))
			{
				return false;
			}
			TestTrue(TEXT("Oodle decompression succeeds"),
				FDRSnowNetworkUtils::DecompressSnapshotData(Compressed, Size, Restored));
			TestTrue(TEXT("Snapshot bytes survive round trip"), Original == Restored);
		}
	}

	TArray<uint8> Empty;
	TArray<uint8> Output = {1, 2, 3};
	TestFalse(TEXT("Empty compression fails"), FDRSnowNetworkUtils::CompressSnapshotData(Empty, Output));
	TestTrue(TEXT("Compression failure clears stale output"), Output.IsEmpty());
	Output = {1, 2, 3};
	TestFalse(TEXT("Empty decompression fails"), FDRSnowNetworkUtils::DecompressSnapshotData(Empty, 16, Output));
	TestTrue(TEXT("Decompression failure clears stale output"), Output.IsEmpty());
	const TArray<uint8> Payload = {1};
	for (const int32 InvalidSize : {0, -1})
	{
		Output = {1, 2, 3};
		TestFalse(TEXT("Invalid original size fails"),
			FDRSnowNetworkUtils::DecompressSnapshotData(Payload, InvalidSize, Output));
		TestTrue(TEXT("Invalid size clears stale output"), Output.IsEmpty());
	}
	return true;
}
#endif
