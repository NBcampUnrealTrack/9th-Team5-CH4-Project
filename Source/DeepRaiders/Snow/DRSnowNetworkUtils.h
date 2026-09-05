#pragma once

#include "CoreMinimal.h"
#include "DRSnowTypes.h"
#include "TimerManager.h"

struct DEEPRAIDERS_API FDRSnowNetSerializeUtils
{
	static uint32 EncodeSignedInt(int32 Value);
	static int32 DecodeSignedInt(uint32 Value);
	static int32 GetPackedUIntSize(uint32 Value);
	static uint32 GetRunLength(const TArray<uint16>& Indices, uint32 Start);
	static bool UseRunEncoding(const TArray<uint16>& Indices, int32& OutIndexBytes);
	static bool SerializeCount(FArchive& Ar, uint32& Value, uint32 Maximum);
};

// 첫 작업은 즉시 전송하고, 쿨타임 중 들어온 작업만 다음 전송으로 묶는다.
struct DEEPRAIDERS_API FDRSnowOperationBatcher : public TSharedFromThis<FDRSnowOperationBatcher>
{
	FDRSnowOperationBatcher(
		UWorld& InWorld,
		TFunction<void(const TArray<FDRSnowOperationRecord>&)> InBroadcast);
	~FDRSnowOperationBatcher();

	void Enqueue(FDRSnowOperationRecord&& Record);
	void Reset();

	static constexpr float CooldownSeconds = 0.05f;
	static constexpr int32 MaxOperationsPerBatch = 16;
	static constexpr int32 MaxBatchesPerFlush = 4;

private:
	void Flush();

	TWeakObjectPtr<UWorld> World;
	TFunction<void(const TArray<FDRSnowOperationRecord>&)> Broadcast;
	TArray<FDRSnowOperationRecord> PendingOperations;
	FTimerHandle CooldownTimer;
	uint32 Generation = 0;
};
