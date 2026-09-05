#include "DRSnowNetworkUtils.h"

#include "Engine/World.h"

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
	Timers.SetTimer(CooldownTimer,
		FTimerDelegate::CreateSP(AsShared(), &FDRSnowOperationBatcher::Flush),
		CooldownSeconds, false);
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

uint32 FDRSnowNetSerializeUtils::EncodeSignedInt(const int32 Value)
{
	return (static_cast<uint32>(Value) << 1) ^ static_cast<uint32>(Value >> 31);
}

int32 FDRSnowNetSerializeUtils::DecodeSignedInt(const uint32 Value)
{
	return static_cast<int32>((Value >> 1) ^ -static_cast<int32>(Value & 1));
}

int32 FDRSnowNetSerializeUtils::GetPackedUIntSize(uint32 Value)
{
	int32 ByteCount = 1;
	while (Value >= 0x80)
	{
		Value >>= 7;
		++ByteCount;
	}
	return ByteCount;
}

uint32 FDRSnowNetSerializeUtils::GetRunLength(const TArray<uint16>& Indices, const uint32 Start)
{
	uint32 End = Start + 1;
	while (End < static_cast<uint32>(Indices.Num()) && Indices[End] == Indices[End - 1] + 1)
	{
		++End;
	}
	return End - Start;
}

bool FDRSnowNetSerializeUtils::UseRunEncoding(const TArray<uint16>& Indices, int32& OutIndexBytes)
{
	int32 DeltaBytes = 0;
	int32 RunBytes = 0;
	uint32 Previous = 0;
	for (const uint16 Index : Indices)
	{
		DeltaBytes += FDRSnowNetSerializeUtils::GetPackedUIntSize(Index - Previous);
		Previous = Index;
	}
	Previous = 0;
	for (uint32 Start = 0; Start < static_cast<uint32>(Indices.Num());)
	{
		const uint32 Length = FDRSnowNetSerializeUtils::GetRunLength(Indices, Start);
		RunBytes += FDRSnowNetSerializeUtils::GetPackedUIntSize(Indices[Start] - Previous) + FDRSnowNetSerializeUtils::GetPackedUIntSize(Length - 1);
		Previous = Indices[Start + Length - 1];
		Start += Length;
	}
	OutIndexBytes = FMath::Min(DeltaBytes, RunBytes);
	return RunBytes < DeltaBytes;
}

bool FDRSnowNetSerializeUtils::SerializeCount(FArchive& Ar, uint32& Value, const uint32 Maximum)
{
	Ar.SerializeIntPacked(Value);
	if (Value > Maximum)
	{
		Ar.SetError();
		return false;
	}
	return !Ar.IsError();
}
