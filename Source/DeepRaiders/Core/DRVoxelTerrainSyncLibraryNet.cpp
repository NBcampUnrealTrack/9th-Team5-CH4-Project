#include "DRVoxelTerrainSyncLibrary.h"

#include "DRVoxelTerrainAreaSyncActor.h"

namespace
{
	// signed 좌표를 packed unsigned 정수로 보내기 위한 ZigZag 인코딩이다.
	// 0, -1, 1, -2, 2가 각각 작은 unsigned 값에 배치되어 원점 근처의 음수 청크도 짧게 전송된다.
	uint32 EncodeSignedInt(int32 Value)
	{
		return Value >= 0
			? static_cast<uint32>(Value) * 2u
			: static_cast<uint32>(-static_cast<int64>(Value) * 2 - 1);
	}

	int32 DecodeSignedInt(uint32 Value)
	{
		return (Value & 1u) == 0u
			? static_cast<int32>(Value >> 1u)
			: static_cast<int32>(-static_cast<int64>((Value >> 1u) + 1u));
	}

	void SerializeSignedIntPacked(FArchive& Ar, int32& Value)
	{
		// 저장과 로드가 같은 함수 경로를 사용한다. 로드 시에는 먼저 packed 값을 읽고 원래 부호를 복원한다.
		uint32 PackedValue = Ar.IsSaving() ? EncodeSignedInt(Value) : 0u;
		Ar.SerializeIntPacked(PackedValue);
		if (Ar.IsLoading())
		{
			Value = DecodeSignedInt(PackedValue);
		}
	}

	bool FailSerialization(FArchive& Ar, bool& bOutSuccess)
	{
		// 모든 검증 실패가 동일하게 Archive 오류와 false 결과를 남기게 하는 공통 종료 경로다.
		Ar.SetError();
		bOutSuccess = false;
		return false;
	}
}

bool FDRTerrainEditFastArrayItem::NetSerialize(
	FArchive& Ar,
	UPackageMap* Map,
	bool& bOutSuccess)
{
	bOutSuccess = true;

	// 현재 편집 종류는 두 개뿐이므로 1bit면 충분하다. 로드 시 미래/손상 값을 그대로 enum으로 쓰지 않는다.
	uint8 TypeValue = static_cast<uint8>(Type);
	Ar.SerializeBits(&TypeValue, 1);
	if (Ar.IsLoading())
	{
		if (TypeValue > static_cast<uint8>(EDRTerrainEditType::Dig))
		{
			return FailSerialization(Ar, bOutSuccess);
		}
		Type = static_cast<EDRTerrainEditType>(TypeValue);
	}

	// Revision은 음수가 될 수 없는 단조 증가 값이므로 packed unsigned 표현을 사용한다.
	uint32 PackedRevision = Ar.IsSaving() ? static_cast<uint32>(FMath::Max(0, Revision)) : 0u;
	Ar.SerializeIntPacked(PackedRevision);
	if (Ar.IsLoading())
	{
		Revision = static_cast<int32>(PackedRevision);
	}

	if (Type == EDRTerrainEditType::Dig)
	{
		// Union처럼 사용하는 구조체이므로 로드 시 반대 타입 payload를 명시적으로 비운다.
		// Radius는 유한한 양수만 허용해 잘못된 네트워크 입력이 월드 편집으로 이어지지 않게 한다.
		if (Ar.IsLoading())
		{
			ChunkCoordinate = FIntVector::ZeroValue;
			MaterialIndex = 0;
			Deltas.Reset();
		}
		bool bLocationSuccess = true;
		Location.NetSerialize(Ar, Map, bLocationSuccess);
		Ar << Radius;
		bOutSuccess = bLocationSuccess && FMath::IsFinite(Radius) && Radius > 0.f;
		return bOutSuccess;
	}

	// Deposit payload: 부호 있는 청크 좌표 3개, 8bit 머터리얼, 정렬된 복셀 델타 목록.
	SerializeSignedIntPacked(Ar, ChunkCoordinate.X);
	SerializeSignedIntPacked(Ar, ChunkCoordinate.Y);
	SerializeSignedIntPacked(Ar, ChunkCoordinate.Z);
	Ar.SerializeBits(&MaterialIndex, 8);
	if (Ar.IsLoading())
	{
		Location = FVector::ZeroVector;
		Radius = 0.f;
	}

	// 단일 레코드는 32^3 청크 하나를 넘을 수 없다. 로드 전에 개수를 검증해 과도한 배열 할당도 막는다.
	uint32 DeltaCount = Ar.IsSaving() ? static_cast<uint32>(Deltas.Num()) : 0u;
	Ar.SerializeIntPacked(DeltaCount);
	if (DeltaCount > static_cast<uint32>(DRVoxelTerrainSync::NetworkChunkVoxelCount))
	{
		return FailSerialization(Ar, bOutSuccess);
	}
	if (Ar.IsLoading())
	{
		Deltas.SetNum(static_cast<int32>(DeltaCount));
	}

	int32 PreviousIndex = -1;
	for (uint32 DeltaIndex = 0; DeltaIndex < DeltaCount; ++DeltaIndex)
	{
		FDRVoxelCompressedValueDelta& Delta = Deltas[static_cast<int32>(DeltaIndex)];
		uint32 PackedIndexDelta = 0u;
		if (Ar.IsSaving())
		{
			// LocalIndex가 엄격한 오름차순이어야 이전 인덱스와의 양의 차이만 전송할 수 있다.
			// 정렬/중복 제거는 ConsumeDepositBatch가 담당하지만 직렬화 경계에서도 다시 검증한다.
			if (Delta.LocalIndex <= PreviousIndex ||
				Delta.LocalIndex >= DRVoxelTerrainSync::NetworkChunkVoxelCount)
			{
				return FailSerialization(Ar, bOutSuccess);
			}
			PackedIndexDelta = static_cast<uint32>(Delta.LocalIndex - PreviousIndex - 1);
		}

		Ar.SerializeIntPacked(PackedIndexDelta);
		if (Ar.IsLoading())
		{
			// 누적 덧셈은 int64로 수행해 손상된 packed 값이 int32를 overflow한 뒤 검사를 통과하지 않게 한다.
			const int64 DecodedIndex =
				static_cast<int64>(PreviousIndex) + PackedIndexDelta + 1;
			if (DecodedIndex < 0 || DecodedIndex >= DRVoxelTerrainSync::NetworkChunkVoxelCount)
			{
				return FailSerialization(Ar, bOutSuccess);
			}
			Delta.LocalIndex = static_cast<int32>(DecodedIndex);
		}

		// 복셀 값은 지형 조작 라이브러리와 같은 양자화 범위의 signed 16bit를 비트 그대로 전송한다.
		uint16 PackedValue = Ar.IsSaving()
			? static_cast<uint16>(static_cast<int16>(FMath::Clamp(
				Delta.QuantizedValue,
				-DRVoxelTerrain::QuantizedValueMax,
				DRVoxelTerrain::QuantizedValueMax)))
			: 0u;
		Ar.SerializeBits(&PackedValue, 16);
		if (Ar.IsLoading())
		{
			Delta.QuantizedValue = static_cast<int16>(PackedValue);
		}
		PreviousIndex = Delta.LocalIndex;
	}

	return true;
}

void FDRTerrainEditFastArray::NotifyOwner()
{
	// FastArray 콜백은 데이터 도착 사실만 전달한다. 실제 Revision 연속성 검사와 월드 적용은
	// Actor를 거쳐 SyncLibrary 한 경로에서 수행해 OnRep 프로퍼티 경로와 동작을 통일한다.
	if (Owner)
	{
		Owner->HandleReplicatedTerrainEdits();
	}
}

void FDRTerrainEditFastArray::PostReplicatedAdd(
	const TArrayView<int32> AddedIndices,
	int32 FinalSize)
{
	(void)AddedIndices;
	(void)FinalSize;
	NotifyOwner();
}

void FDRTerrainEditFastArray::PostReplicatedChange(
	const TArrayView<int32> ChangedIndices,
	int32 FinalSize)
{
	(void)ChangedIndices;
	(void)FinalSize;
	NotifyOwner();
}
