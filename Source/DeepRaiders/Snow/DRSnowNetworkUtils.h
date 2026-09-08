#pragma once

#include "CoreMinimal.h"
#include "DRSnowTypes.h"
#include "TimerManager.h"

// 첫 작업은 즉시 전송하고, 쿨타임 중 들어온 작업만 다음 전송으로 묶는다.
struct DEEPRAIDERS_API FDRSnowOperationBatcher : public TSharedFromThis<FDRSnowOperationBatcher>
{
	FDRSnowOperationBatcher(
		UWorld& InWorld,
		TFunction<void(const TArray<FDRSnowOperationRecord>&)> InBroadcast);
	~FDRSnowOperationBatcher();

	void Enqueue(FDRSnowOperationRecord&& Record);
	void Reset();

	static constexpr float CooldownSeconds = 0.0f;
	static constexpr int32 MaxOperationsPerBatch = 16;
	static constexpr int32 MaxBatchesPerFlush = 8;

private:
	void Flush();

	TWeakObjectPtr<UWorld> World;
	TFunction<void(const TArray<FDRSnowOperationRecord>&)> Broadcast;
	TArray<FDRSnowOperationRecord> PendingOperations;
	FTimerHandle CooldownTimer;
	uint32 Generation = 0;
};

class DEEPRAIDERS_API FDRSnowNetworkUtils
{
public:
	// 송수신 모두 Oodle 포맷을 사용한다. 입력/출력은 서로 다른 배열이어야 한다.
	// 실패하면 출력은 비운다. 원본 크기는 BeginSnapshot 메타데이터로 별도 전달한다.
	static bool CompressSnapshotData(const TArray<uint8>& UncompressedData, TArray<uint8>& OutCompressedData);
	static bool DecompressSnapshotData(const TArray<uint8>& CompressedData, int32 ExpectedUncompressedSize, TArray<uint8>& OutUncompressedData);
};
