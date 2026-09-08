#include "DRSnowNetworkUtils.h"

#include "Engine/World.h"
#include "Misc/Compression.h"

FDRSnowOperationBatcher::FDRSnowOperationBatcher(
	UWorld& InWorld,
	TFunction<void(const TArray<FDRSnowOperationRecord>&)> InBroadcast)
	: World(&InWorld)
	, Broadcast(MoveTemp(InBroadcast))
{
}

FDRSnowOperationBatcher::~FDRSnowOperationBatcher()
{
	Reset();
}

void FDRSnowOperationBatcher::Enqueue(FDRSnowOperationRecord&& Record)
{
	check(IsInGameThread());
	UWorld* ValidWorld = World.Get();
	if (!IsValid(ValidWorld))
	{
		return;
	}
	PendingOperations.Add(MoveTemp(Record));
	if (!ValidWorld->GetTimerManager().IsTimerActive(CooldownTimer))
	{
		Flush();
	}
}

void FDRSnowOperationBatcher::Reset()
{
	++Generation;
	if (UWorld* ValidWorld = World.Get())
	{
		ValidWorld->GetTimerManager().ClearTimer(CooldownTimer);
	}
	CooldownTimer.Invalidate();
	PendingOperations.Reset();
}

void FDRSnowOperationBatcher::Flush()
{
	UWorld* ValidWorld = World.Get();
	if (!IsValid(ValidWorld))
	{
		Reset();
		return;
	}
	FTimerManager& Timers = ValidWorld->GetTimerManager();
	Timers.ClearTimer(CooldownTimer);
	if (PendingOperations.IsEmpty())
	{
		return;
	}

	// 콜백에서 재진입해도 즉시 전송을 반복하지 않도록 발송 전에 쿨타임을 건다.
	const FTimerDelegate FlushDelegate =
		FTimerDelegate::CreateSP(AsShared(), &FDRSnowOperationBatcher::Flush);
	if constexpr (CooldownSeconds <= 0.f)
	{
		// SetTimer의 0초는 타이머 해제이므로 다음 tick 예약 API를 사용한다.
		CooldownTimer = Timers.SetTimerForNextTick(FlushDelegate);
	}
	else
	{
		Timers.SetTimer(CooldownTimer, FlushDelegate, CooldownSeconds, false);
	}
	const uint32 FlushGeneration = Generation;
	for (int32 BatchIndex = 0;
		BatchIndex < MaxBatchesPerFlush && !PendingOperations.IsEmpty();
		++BatchIndex)
	{
		const int32 BatchSize = FMath::Min(MaxOperationsPerBatch, PendingOperations.Num());
		TArray<FDRSnowOperationRecord> Batch;
		Batch.Reserve(BatchSize);
		for (int32 Index = 0; Index < BatchSize; ++Index)
		{
			Batch.Add(MoveTemp(PendingOperations[Index]));
		}
		PendingOperations.RemoveAt(0, BatchSize, EAllowShrinking::No);
		Broadcast(Batch);
		if (Generation != FlushGeneration)
		{
			return;
		}
	}
}

bool FDRSnowNetworkUtils::CompressSnapshotData(const TArray<uint8>& UncompressedData, TArray<uint8>& OutCompressedData)
{
	OutCompressedData.Reset();
	if (UncompressedData.IsEmpty())
	{
		return false;
	}

	const int32 UncompressedSize = UncompressedData.Num();
	int64 BufferSize = 0;
	// 압축 결과 최대 크기와 인코더가 요구하는 작업 버퍼 크기는 다르다.
	if (!FCompression::CompressMemoryBound(NAME_Oodle, BufferSize, static_cast<int64>(UncompressedSize))
		|| BufferSize <= 0 || BufferSize > MAX_int32)
	{
		return false;
	}
	OutCompressedData.SetNumUninitialized(static_cast<int32>(BufferSize));

	int64 CompressedSize = BufferSize;
	const bool bSuccess = FCompression::CompressMemory(
		NAME_Oodle,
		OutCompressedData.GetData(),
		CompressedSize,
		UncompressedData.GetData(),
		static_cast<int64>(UncompressedSize),
		COMPRESS_BiasSize
	);

	if (bSuccess && CompressedSize > 0 && CompressedSize <= BufferSize)
	{
		OutCompressedData.SetNum(static_cast<int32>(CompressedSize));
		return true;
	}

	OutCompressedData.Empty();
	return false;
}

bool FDRSnowNetworkUtils::DecompressSnapshotData(const TArray<uint8>& CompressedData, int32 ExpectedUncompressedSize, TArray<uint8>& OutUncompressedData)
{
	OutUncompressedData.Reset();
	if (CompressedData.IsEmpty() || ExpectedUncompressedSize <= 0)
	{
		return false;
	}

	int64 MaxCompressedSize = 0;
	if (!FCompression::GetMaximumCompressedSize(NAME_Oodle, MaxCompressedSize,
		static_cast<int64>(ExpectedUncompressedSize)) || CompressedData.Num() > MaxCompressedSize)
	{
		return false;
	}

	OutUncompressedData.SetNumUninitialized(ExpectedUncompressedSize);

	const bool bSuccess = FCompression::UncompressMemory(
		NAME_Oodle,
		OutUncompressedData.GetData(),
		ExpectedUncompressedSize,
		CompressedData.GetData(),
		CompressedData.Num()
	);

	if (!bSuccess)
	{
		OutUncompressedData.Empty();
	}

	return bSuccess;
}
