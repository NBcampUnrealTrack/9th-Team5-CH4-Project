#pragma once

#include "CoreMinimal.h"
#include "DRVoxelDepositOperations.generated.h"

class AActor;
class AVoxelWorld;
class UWorld;

namespace DRVoxelDeposit
{
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
	 * @param RandomStream 난수를 소비할 스트림입니다.
	 * @param OutIndices 선택된 인덱스 배열입니다.
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
	 * @param OutAmountScale 계산된 퇴적량 배율입니다.
	 * @param OutAllowedHeightDelta 계산된 허용 높이 차입니다.
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

USTRUCT()
struct FDRVoxelDepositInBoxSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float SurfaceSampleSpacing = 50.f;

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositAmountPerPass = 0.05f;

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="1"))
	int32 RandomSurfaceSampleCount = 16;

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="1"))
	int32 MaxSelectedSurfaceCount = 4;

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositSpreadRadius = 100.f;

	UPROPERTY()
	int32 RandomSeed = 0;
};

USTRUCT()
struct FDRVoxelDepositCommand
{
	GENERATED_BODY()

	UPROPERTY()
	FVector ScanCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector ScanExtent = FVector::ZeroVector;

	UPROPERTY()
	FVector AreaCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector AreaExtent = FVector::ZeroVector;

	UPROPERTY()
	FDRVoxelDepositInBoxSettings Settings;

	UPROPERTY()
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	UPROPERTY()
	float MaxStaticMeshSlopeAngle = 50.f;

	UPROPERTY()
	bool bDepositOnStaticMeshes = false;

	UPROPERTY()
	bool bTraceComplexStaticMeshSurfaces = false;
};

struct FDRVoxelDepositWrite
{
	FIntVector Position = FIntVector::ZeroValue;
	float AmountScale = 1.f;
	float StaticMeshSurfaceZ = 0.f;
	bool bHasStaticMeshSupport = false;
};

struct FDRVoxelDepositPlan
{
	FDRVoxelDepositInBoxSettings Settings;
	FIntVector WriteVoxelMin = FIntVector::ZeroValue;
	FIntVector WriteVoxelMax = FIntVector::ZeroValue;
	TArray<FDRVoxelDepositWrite> Writes;

	bool IsEmpty() const
	{
		return Writes.IsEmpty();
	}

	void Reset()
	{
		*this = FDRVoxelDepositPlan();
	}
};

class DEEPRAIDERS_API FDRVoxelDepositOperations
{
public:
	/**
	 * 표면을 검사하고 다음 프레임에 적용할 퇴적 계획을 만듭니다.
	 * @param World StaticMesh 트레이스에 사용할 월드입니다.
	 * @param VoxelWorld 표면을 검사할 복셀 월드입니다.
	 * @param TraceOwner 트레이스에서 제외할 액터입니다.
	 * @param Command 실행할 퇴적 명령입니다.
	 * @param OutPlan 생성된 퇴적 계획입니다.
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
	 * @param VoxelWorld 계획을 적용할 복셀 월드입니다.
	 * @param Plan 적용 후 초기화할 퇴적 계획입니다.
	 * @return 계획 적용 또는 빈 계획 처리가 성공하면 true입니다.
	 */
	static bool ApplyDepositPlan(
		AVoxelWorld* VoxelWorld,
		FDRVoxelDepositPlan& Plan);

};
