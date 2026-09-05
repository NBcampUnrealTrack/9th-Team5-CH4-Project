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
