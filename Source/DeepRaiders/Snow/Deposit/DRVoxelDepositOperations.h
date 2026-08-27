#pragma once

#include "CoreMinimal.h"
#include "DRVoxelDepositOperations.generated.h"

class AActor;
class AVoxelWorld;
class UWorld;

namespace DRVoxelDeposit
{
	/**
	 * 배열을 전달받은 난수 스트림으로 섞습니다.
	 * @tparam ElementType 배열 요소 형식입니다.
	 * @param[in,out] Values 섞을 배열입니다.
	 * @param[in,out] RandomStream 난수를 소비할 스트림입니다.
	 */
	template<typename ElementType>
	void ShuffleArray(TArray<ElementType>& Values, FRandomStream& RandomStream)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			Values.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}

	/**
	 * 전체 배열을 만들지 않고 중복 없는 무작위 인덱스를 선택합니다.
	 * @param PopulationSize 선택 가능한 전체 인덱스 수입니다.
	 * @param RequestedCount 선택할 인덱스 수입니다.
	 * @param[in,out] RandomStream 난수를 소비할 스트림입니다.
	 * @param[out] OutIndices 선택된 인덱스 배열입니다.
	 */
	inline void BuildRandomUniqueIndices(
		int32 PopulationSize,
		int32 RequestedCount,
		FRandomStream& RandomStream,
		TArray<int32>& OutIndices)
	{
		OutIndices.Reset();
		if (PopulationSize <= 0 || RequestedCount <= 0)
		{
			return;
		}

		const int32 SampleCount = FMath::Clamp(RequestedCount, 0, PopulationSize);
		TSet<int32> SelectedIndices;
		SelectedIndices.Reserve(SampleCount);
		OutIndices.Reserve(SampleCount);
		for (int32 UpperBound = PopulationSize - SampleCount;
			UpperBound < PopulationSize;
			++UpperBound)
		{
			const int32 Candidate = RandomStream.RandRange(0, UpperBound);
			const int32 SelectedIndex = SelectedIndices.Contains(Candidate)
				? UpperBound
				: Candidate;
			SelectedIndices.Add(SelectedIndex);
			OutIndices.Add(SelectedIndex);
		}

		ShuffleArray(OutIndices, RandomStream);
	}

	/**
	 * 원형 풋프린트 포함 여부와 거리 기반 값을 계산합니다.
	 * @param OffsetX 중심 기준 X 오프셋입니다.
	 * @param OffsetY 중심 기준 Y 오프셋입니다.
	 * @param Radius 풋프린트 반경입니다.
	 * @param EdgeStrength 가장자리 퇴적량 배율입니다.
	 * @param MaximumSlopeTangent 허용할 최대 경사의 탄젠트 값입니다.
	 * @param[out] OutAmountScale 계산된 퇴적량 배율입니다.
	 * @param[out] OutAllowedHeightDelta 계산된 허용 높이 차입니다.
	 * @return 오프셋이 원형 풋프린트 안에 있으면 true입니다.
	 */
	inline bool EvaluateFootprintOffset(
		int32 OffsetX,
		int32 OffsetY,
		int32 Radius,
		float EdgeStrength,
		float MaximumSlopeTangent,
		float& OutAmountScale,
		float& OutAllowedHeightDelta)
	{
		const float RadiusAsFloat = static_cast<float>(FMath::Max(1, Radius));
		const float Distance = FMath::Sqrt(
			static_cast<float>(OffsetX * OffsetX + OffsetY * OffsetY));
		if (Radius > 0 && Distance > RadiusAsFloat + 0.5f)
		{
			return false;
		}

		const float DistanceAlpha = Radius > 0
			? FMath::Clamp(Distance / RadiusAsFloat, 0.f, 1.f)
			: 0.f;
		OutAmountScale = FMath::Lerp(1.f, EdgeStrength, DistanceAlpha);
		OutAllowedHeightDelta = FMath::Max(
			1.f,
			Distance * MaximumSlopeTangent + 0.5f);
		return true;
	}
}

/** 한 번의 퇴적 요청에 사용하는 설정입니다. */
USTRUCT()
struct FDRVoxelDepositInBoxSettings
{
	GENERATED_BODY()

	/** 표면 후보를 찾는 X/Y 격자의 월드 간격입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float SurfaceSampleSpacing = 50.f;

	/** 한 번의 퇴적에서 복셀 밀도값을 낮출 기본량입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositAmountPerPass = 0.05f;

	/** 퇴적 복셀에 기록할 머터리얼 인덱스입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	/** 복셀과 StaticMesh가 공유하는 무작위 X/Y 표본 수입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="1"))
	int32 RandomSurfaceSampleCount = 16;

	/** 실제 풋프린트로 확장할 표면 중심의 최대 개수입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="1"))
	int32 MaxSelectedSurfaceCount = 4;

	/** 선택한 표면 중심에서 눈이 퍼지는 월드 반경입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositSpreadRadius = 100.f;

	/** 모든 인스턴스가 동일한 결과를 재현하는 난수 시드입니다. */
	UPROPERTY()
	int32 RandomSeed = 0;
};

/** 복셀 결과 대신 RPC로 전달하는 결정적 퇴적 명령입니다. */
USTRUCT()
struct FDRVoxelDepositCommand
{
	GENERATED_BODY()

	/** 이번 요청에서 표면을 검사할 월드 영역의 중심입니다. */
	UPROPERTY()
	FVector ScanCenter = FVector::ZeroVector;

	/** 표면 검사 영역의 월드 축 정렬 반크기입니다. */
	UPROPERTY()
	FVector ScanExtent = FVector::ZeroVector;

	/** 최종 퇴적을 제한하는 전체 관리 영역의 중심입니다. */
	UPROPERTY()
	FVector AreaCenter = FVector::ZeroVector;

	/** 최종 퇴적을 제한하는 전체 관리 영역의 반크기입니다. */
	UPROPERTY()
	FVector AreaExtent = FVector::ZeroVector;

	/** 이번 요청에 적용할 퇴적 설정입니다. */
	UPROPERTY()
	FDRVoxelDepositInBoxSettings Settings;

	/** 지정 시 같은 태그를 가진 StaticMesh만 표면으로 허용합니다. */
	UPROPERTY()
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	/** 퇴적을 허용할 StaticMesh 표면의 최대 경사각입니다. */
	UPROPERTY()
	float MaxStaticMeshSlopeAngle = 50.f;

	/** StaticMesh 표면 퇴적 사용 여부입니다. */
	UPROPERTY()
	bool bDepositOnStaticMeshes = false;

	/** StaticMesh 검사에 복잡 충돌을 사용할지 결정합니다. */
	UPROPERTY()
	bool bTraceComplexStaticMeshSurfaces = false;
};

/** 다음 프레임에 실행할 복셀 쓰기 한 건입니다. */
struct FDRVoxelDepositWrite
{
	/** 값을 변경할 로컬 복셀 좌표입니다. */
	FIntVector Position = FIntVector::ZeroValue;
	/** 중심 거리로 계산한 퇴적량 배율입니다. */
	float AmountScale = 1.f;
	/** StaticMesh 지지면의 로컬 Z 좌표입니다. */
	float StaticMeshSurfaceZ = 0.f;
	/** StaticMesh가 해당 복셀의 지지면인지 나타냅니다. */
	bool bHasStaticMeshSupport = false;
};

/** 준비 프레임에서 만든 일회용 퇴적 계획입니다. */
struct FDRVoxelDepositPlan
{
	/** 쓰기 단계에 필요한 퇴적 설정입니다. */
	FDRVoxelDepositInBoxSettings Settings;
	/** 쓰기를 허용하는 최소 로컬 복셀 좌표입니다. */
	FIntVector WriteVoxelMin = FIntVector::ZeroValue;
	/** 쓰기를 허용하는 최대 로컬 복셀 좌표입니다. */
	FIntVector WriteVoxelMax = FIntVector::ZeroValue;
	/** 중복이 제거된 최종 복셀 쓰기 목록입니다. */
	TArray<FDRVoxelDepositWrite> Writes;

	/**
	 * 실행할 쓰기가 없는지 확인합니다.
	 * @return 쓰기 목록이 비어 있으면 true입니다.
	 */
	bool IsEmpty() const
	{
		return Writes.IsEmpty();
	}

	/** 계획을 기본 상태로 초기화합니다. */
	void Reset()
	{
		*this = FDRVoxelDepositPlan();
	}
};

/** 퇴적 준비와 적용을 수행하는 순수 C++ 연산 모음입니다. */
class DEEPRAIDERS_API FDRVoxelDepositOperations
{
public:
	/**
	 * 표면을 검사하고 다음 프레임에 적용할 퇴적 계획을 만듭니다.
	 * @param[in] World StaticMesh 트레이스에 사용할 월드입니다.
	 * @param[in] VoxelWorld 표면을 검사할 복셀 월드입니다.
	 * @param[in] TraceOwner 트레이스에서 제외할 액터입니다.
	 * @param[in] Command 실행할 퇴적 명령입니다.
	 * @param[out] OutPlan 생성된 퇴적 계획입니다.
	 * @return 계획 생성이 완료되면 true이며 빈 계획도 성공입니다.
	 */
	static bool PrepareDepositCommand(
		UWorld* World,
		AVoxelWorld* VoxelWorld,
		AActor* TraceOwner,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelDepositPlan& OutPlan);

	/**
	 * 준비된 계획을 복셀 월드에 적용하고 계획을 비웁니다.
	 * @param[in] VoxelWorld 계획을 적용할 복셀 월드입니다.
	 * @param[in,out] Plan 적용 후 초기화할 퇴적 계획입니다.
	 * @return 계획 적용 또는 빈 계획 처리가 성공하면 true입니다.
	 */
	static bool ApplyDepositPlan(
		AVoxelWorld* VoxelWorld,
		FDRVoxelDepositPlan& Plan);

};
