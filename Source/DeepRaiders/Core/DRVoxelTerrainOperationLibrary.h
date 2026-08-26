#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldCollision.h"
#include "DRVoxelTerrainOperationLibrary.generated.h"

class AActor;
class AVoxelWorld;
class UWorld;
class UDRVoxelTerrainOperationLibrary;

// 지형 조작과 wire-format 복원에서 공유하는 좌표/밀도 계산 규칙이다.
namespace DRVoxelTerrain
{
	inline constexpr int32 QuantizedValueMax = 32767;

	struct FInclusiveVoxelBoxDimensions
	{
		int32 SizeX = 0;
		int32 SizeXY = 0;
		int32 TotalVoxelCount = 0;
	};

	inline bool TryGetInclusiveVoxelBoxDimensions(
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax,
		FInclusiveVoxelBoxDimensions& OutDimensions,
		bool bRequireExpandableBounds = false)
	{
		OutDimensions = FInclusiveVoxelBoxDimensions();
		if (bRequireExpandableBounds &&
			(VoxelMin.X == MIN_int32 || VoxelMin.Y == MIN_int32 || VoxelMin.Z == MIN_int32 ||
				VoxelMax.X == MAX_int32 || VoxelMax.Y == MAX_int32 || VoxelMax.Z == MAX_int32))
		{
			return false;
		}

		const int64 SizeX = static_cast<int64>(VoxelMax.X) - VoxelMin.X + 1;
		const int64 SizeY = static_cast<int64>(VoxelMax.Y) - VoxelMin.Y + 1;
		const int64 SizeZ = static_cast<int64>(VoxelMax.Z) - VoxelMin.Z + 1;
		if (SizeX <= 0 || SizeY <= 0 || SizeZ <= 0 ||
			SizeX > MAX_int32 || SizeY > MAX_int32 || SizeZ > MAX_int32 ||
			SizeX > MAX_int32 / SizeY || SizeX * SizeY > MAX_int32 / SizeZ)
		{
			return false;
		}

		OutDimensions.SizeX = static_cast<int32>(SizeX);
		OutDimensions.SizeXY = static_cast<int32>(SizeX * SizeY);
		OutDimensions.TotalVoxelCount = static_cast<int32>(SizeX * SizeY * SizeZ);
		return true;
	}

	inline int32 GetInclusiveVoxelLocalIndex(
		const FIntVector& Position,
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax)
	{
		FInclusiveVoxelBoxDimensions Dimensions;
		const bool bHasValidDimensions = TryGetInclusiveVoxelBoxDimensions(
			VoxelMin,
			VoxelMax,
			Dimensions);
		check(bHasValidDimensions);
		const int64 LocalIndex =
			(Position.X - VoxelMin.X) +
			static_cast<int64>(Position.Y - VoxelMin.Y) * Dimensions.SizeX +
			static_cast<int64>(Position.Z - VoxelMin.Z) * Dimensions.SizeXY;
		check(LocalIndex >= 0 && LocalIndex < Dimensions.TotalVoxelCount);
		return static_cast<int32>(LocalIndex);
	}

	inline FIntVector GetInclusiveVoxelPosition(
		int32 LocalIndex,
		const FIntVector& VoxelMin,
		const FInclusiveVoxelBoxDimensions& Dimensions)
	{
		check(LocalIndex >= 0 && LocalIndex < Dimensions.TotalVoxelCount);
		const int32 LocalZ = LocalIndex / Dimensions.SizeXY;
		const int32 Remainder = LocalIndex % Dimensions.SizeXY;
		const int32 LocalY = Remainder / Dimensions.SizeX;
		const int32 LocalX = Remainder % Dimensions.SizeX;
		return VoxelMin + FIntVector(LocalX, LocalY, LocalZ);
	}

	template<typename ElementType>
	void ShuffleArray(TArray<ElementType>& Values, FRandomStream& RandomStream)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			Values.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}

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

UENUM(BlueprintType)
enum class EDRVoxelDepositRequestPhase : uint8
{
	// 청크의 모든 X/Y 샘플 열을 순회하면서 지표면 위의 퇴적 후보를 수집하는 단계다.
	BuildCandidates,
	// 선택된 중심의 원형 풋프린트 각 칸에서 실제 표면 높이를 다시 찾아 최종 쓰기 위치를 만드는 단계다.
	ResolveFootprints,
	// 표면 해석이 끝난 위치에 복셀 값과 머터리얼을 기록하는 단계다.
	ApplyVoxels,
	// 모든 샘플 열과 남은 후보를 처리해 요청 배열에서 제거해도 되는 상태다.
	Finished
};

// 복셀 스캔, 쓰기, 물리 트레이스 예산을 하나로 묶는다. 액터는 세부 반복 횟수를 알 필요가 없다.
UENUM(BlueprintType)
enum class EDRDepositPerformancePreset : uint8
{
	Low,
	Balanced,
	High
};

// 관리 박스를 XY로 나눈 표면 처리 청크다. 각 청크는 관리 영역의 전체 Z 범위를 공유한다.
USTRUCT(BlueprintType)
struct FDRVoxelTerrainChunkBounds
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FIntPoint ChunkCoordinate = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FVector BoxCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	FVector BoxExtent = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync|Chunks")
	int32 LastProcessedPass = INDEX_NONE;
};

// 복셀 위치는 박스 내부의 1차원 인덱스로, 값은 정수로 양자화해 전송 크기를 줄인다.
// 머터리얼 인덱스는 레코드 단위로 하나만 보관하므로 각 델타에는 값 변화만 기록한다.
USTRUCT(BlueprintType)
struct FDRVoxelCompressedValueDelta
{
	GENERATED_BODY()

	// VoxelMin을 원점으로 한 X 우선 1차원 인덱스다. 절대 좌표를 보내는 것보다 데이터가 작다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 LocalIndex = 0;

	// [-1, 1] 밀도 값을 [-32767, 32767] 범위 정수로 변환한 최종 값이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 QuantizedValue = 0;
};

// 서버가 한 번의 틱에서 실제로 변경한 퇴적 복셀 묶음이다.
// 클라이언트는 VoxelMin을 기준으로 LocalIndex를 복원한 뒤 같은 값과 머터리얼을 적용한다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositDeltaRecord
{
	GENERATED_BODY()

	// 서버에서 퇴적 또는 굴착이 실제로 발생할 때마다 1씩 증가하는 공통 순서 번호다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 Revision = 0;

	// LocalIndex를 다시 3차원 복셀 좌표로 복원하기 위한 포함 범위의 최솟값과 최댓값이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMax = FIntVector::ZeroValue;

	// 이 레코드에 들어 있는 모든 복셀에 공통으로 적용할 단일 머터리얼 인덱스다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	uint8 MaterialIndex = 0;

	// 이번 서버 틱에서 실제 값이 달라진 복셀만 담는다. 실패한 쓰기 시도는 포함하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TArray<FDRVoxelCompressedValueDelta> Deltas;
};

// 굴착은 이미 서버의 다른 시스템에서 처리되므로 여기에는 재현에 필요한 구 중심과 반지름만 저장한다.
// 퇴적 레코드와 같은 Revision 흐름을 사용해 "쌓기 -> 굴착" 같은 편집 순서를 클라이언트에서도 보존한다.
USTRUCT(BlueprintType)
struct FDRVoxelDigDeltaRecord
{
	GENERATED_BODY()

	// 퇴적 레코드와 함께 정렬할 서버 편집 순서 번호다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Dig")
	int32 Revision = 0;

	// 네트워크 전송 크기를 줄이기 위해 양자화되는 월드 공간 굴착 중심이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Dig")
	FVector_NetQuantize Location = FVector::ZeroVector;

	// 서버에서 실행된 RemoveSphere와 같은 크기로 클라이언트가 재생할 반지름이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Dig")
	float Radius = 0.f;
};

// 한 청크의 퇴적 요청에서 실제로 조절할 필요가 있는 값만 모은 설정이다.
// 지터, 가장자리 감쇠, 최상단 표면 전용 처리 같은 구현 세부값은 내부 기본값으로 고정한다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxSettings
{
	GENERATED_BODY()

	// 월드 단위의 X/Y 표면 샘플 간격이다. 작을수록 촘촘하지만 청크당 조사할 열이 증가한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float SurfaceSampleSpacing = 50.f;

	// 이번 청크 패스에서 선택된 위치의 밀도값에서 뺄 양이다. 값이 클수록 한 번에 두껍게 쌓인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositAmountPerPass = 0.05f;

	// 새로 쌓인 복셀에 기록할 단일 머터리얼 인덱스다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	// 청크에서 발견된 유효 표면 중 이번 패스에 실제로 선택할 비율이다.
	// 독립 확률이 아니라 정확한 목표 개수를 계산하므로 청크마다 누적 면적이 일정하다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="100.0"))
	float SurfaceCoveragePercentPerPass = 10.f;

	// 선택된 표면 하나가 주변으로 퍼지는 월드 단위 반경이다. 요청 생성 시 VoxelSize 기준 정수 반경으로 변환된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositSpreadRadius = 100.f;

	// 0이면 모든 높이를 동일하게 선택하고, 1이면 낮은 표면이 가중 랜덤 선택에서 훨씬 유리해진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LowAreaPreference = 0.75f;

	// 에디터 옵션이 아니라 요청마다 액터가 내부적으로 정하는 결정적 난수 Seed다.
	int32 RandomSeed = 0;
};

// 여러 틱에 걸쳐 처리되는 퇴적 작업의 진행 상태다.
// 요청 생성 후 전체 후보 스캔과 선택 후보 쓰기를 각각 틱 예산만큼 처리하고 완료되면 배열에서 제거된다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxRequest
{
	GENERATED_BODY()

	// 이 요청이 읽고 쓸 대상 월드다. 처리 도중 파괴될 수 있으므로 매 틱 유효성을 다시 검사한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	// 이 청크가 후보 중심을 소유하는 Core 범위다. 표면 스캔과 커버리지 퍼센트 계산은 이 범위만 사용한다.
	// 인접 청크와 겹치지 않으므로 같은 표면 중심이 여러 청크의 후보로 중복 선택되지 않는다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMax = FIntVector::ZeroValue;

	// 선택된 중심의 원형 풋프린트가 실제로 값을 기록할 수 있는 확장 범위다.
	// 기본 요청에서는 Core와 같고, 청크 액터는 퍼짐 반경만큼 확장한 뒤 전체 관리 영역에서 잘라 사용한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector WriteVoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector WriteVoxelMax = FIntVector::ZeroValue;

	// Build 단계에서는 청크 전체에서 발견한 후보를, Resolve 단계에서는 퍼센트 선택을 통과한 중심만 담는다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TArray<FIntVector> PendingVoxels;

	// ResolvedVoxelPositions에서 다음으로 실제 기록할 위치의 인덱스다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextVoxelIndex = 0;

	// 복셀 지형 후보의 풋프린트를 여러 틱에 나누어 해석하기 위한 중심 인덱스와 오프셋 인덱스다.
	// 각 오프셋마다 제한된 Z 범위에서 실제 지표면을 찾으므로 이 진행 상태가 프레임 예산을 보장한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextResolveCandidateIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextResolveOffsetIndex = 0;

	// 섞인 ScanColumnOrder의 현재 항목을 실제 로컬 X/Y 좌표로 변환한 결과다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntPoint ScanCursor = FIntPoint::ZeroValue;

	// 다음으로 처리할 ScanColumnOrder 항목이다. 배열 끝에 도달하면 후보 스캔 전체가 완료된다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextScanColumnIndex = 0;

	// SurfaceSampleSpacing을 VoxelSize로 나눈 정수 복셀 간격이며 최소값은 1이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 VoxelSampleStep = 1;

	// DepositSpreadRadius를 VoxelSize로 나눈 실제 원형 풋프린트 반경이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 DepositFootprintRadius = 0;

	// 청크에서 발견한 전체 유효 표면 수와 퍼센트 선택 후 실제 적용 대상으로 남긴 수다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 DetectedSurfaceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 SelectedSurfaceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FDRVoxelDepositInBoxSettings DepositSettings;

	// 요청 생성 검증을 통과했는지 나타낸다. false인 요청은 처리 배열에서 즉시 제거된다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bIsValid = false;

	// 다음 틱에 수행할 상태 머신 단계다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	EDRVoxelDepositRequestPhase Phase = EDRVoxelDepositRequestPhase::BuildCandidates;

	// 아래 필드는 서버가 요청을 처리하는 동안만 사용하는 런타임 상태다.
	// 최종 동기화에는 실제로 변경된 압축 델타만 사용하므로 이 상태 자체는 복제하지 않는다.
	FRandomStream RandomStream;
	int32 ScanColumnCountY = 0;
	// X/Y 열의 선형 인덱스를 요청 생성 시 한 번 섞어 둔다.
	// 틱마다 앞에서부터 처리해도 한쪽 방향으로 줄무늬처럼 쌓이는 현상을 피할 수 있다.
	TArray<int32> ScanColumnOrder;
	// 후보 중복과 현재 요청 안에서 이미 기록한 쓰기 중복을 빠르게 제거하기 위한 서버 전용 집합이다.
	TSet<FIntVector> PendingVoxelPositions;
	TSet<FIntVector> WrittenVoxelPositions;
	// 선택 중심의 평평한 Z를 그대로 펼치지 않고, 각 X/Y 열에서 다시 찾은 실제 표면 바로 위 좌표를 저장한다.
	// AmountScale 배열은 같은 인덱스의 중심 거리 감쇠값이며 중복 좌표는 가장 큰 값을 하나만 유지한다.
	TArray<FIntVector> ResolvedVoxelPositions;
	TArray<float> ResolvedAmountScales;
	TMap<FIntVector, int32> ResolvedVoxelIndexByPosition;
	bool bVoxelFootprintsResolved = false;
	bool bExternalFootprintsResolved = true;
	bool bExternalWritesMerged = false;
	// 청크 액터가 한 패스 동안 공유하는 중복 방지 집합을 가리킨다. nullptr이면 요청 내부 집합만 사용한다.
	// 액터 멤버를 가리키는 비소유 포인터이며 요청보다 액터가 오래 살고, 패스 취소 시 요청을 먼저 제거한다.
	TSet<FIntVector>* SharedWrittenVoxelPositions = nullptr;
	// 앞서 처리한 청크의 확장 풋프린트가 건드린 XY 열이다. 그 결과 높아진 표면을 다음 청크가
	// 같은 패스에서 새 후보로 다시 잡지 않도록 후보 생성 단계에서만 확인한다.
	TSet<FIntPoint>* SharedWrittenColumns = nullptr;
	// 퍼센트 선택을 통과한 고정 메시 후보 중심만 남긴다. 이 중심의 풋프린트는 액터가 각 칸마다
	// 비동기 트레이스를 다시 발행해 메시 가장자리와 경사를 실제 형상대로 해석한다.
	TSet<FIntVector> ExternalCandidatePositions;
	// 고정 메시 표면은 복셀 밀도장에 고체로 존재하지 않는다. 키는 메시 바로 위에 쓸 복셀이고,
	// 값은 정밀 트레이스로 측정한 복셀 로컬 좌표계의 실제 메시 표면 Z다. 쓰기 단계는 이 높이로 메시 안쪽의
	// 얇은 지지층과 첫 퇴적층의 밀도값을 계산해 첫 누적부터 등가면이 메시 표면에 붙도록 만든다.
	TMap<FIntVector, float> ExternalSupportSurfaceZByVoxel;
	// 고정 메시의 정밀 풋프린트 셀도 중심에서 멀수록 얇아지도록 셀별 강도를 함께 저장한다.
	TMap<FIntVector, float> ExternalResolvedAmountScaleByVoxel;
	// 복셀 풋프린트는 제한된 높이 범위만 다시 찾으면 높은 천장 아래의 바닥을 잘못 선택할 수 있다.
	// 각 XY 열의 전체 Z 최상단 표면을 요청 동안 캐시해 모든 풋프린트 셀이 같은 가시성 규칙을 사용하게 한다.
	TMap<FIntPoint, int32> TopSurfaceZByColumn;
	// 청크 액터가 활성화하면 최종 복셀 쓰기 후보 위의 WorldStatic 충돌을 검사해 천장 아래 쓰기를 막는다.
	// 외부 메시 표면 후보 자체는 이 검사에서 제외한다.
	float WorldBoxTopZ = 0.f;
	bool bBlockDepositBelowWorldStatic = false;
	bool bTraceComplexWorldStaticOcclusion = false;
};

// 한 번의 호출에 필요한 월드와 액터 설정을 명시적으로 전달한다. 라이브러리가 Actor private 멤버에
// 접근하지 않으므로 지형 계산과 네트워크 생명주기의 경계가 분명해진다.
struct FDRVoxelTerrainOperationContext
{
	UWorld* World = nullptr;
	AVoxelWorld* VoxelWorld = nullptr;
	AActor* TraceOwner = nullptr;
	TArray<FDRVoxelTerrainChunkBounds>* TerrainChunks = nullptr;
	const FDRVoxelDepositInBoxSettings* DepositSettings = nullptr;
	FVector AreaCenter = FVector::ZeroVector;
	FVector AreaExtent = FVector::ZeroVector;
	EDRDepositPerformancePreset PerformancePreset = EDRDepositPerformancePreset::Balanced;
	FName RequiredStaticMeshSurfaceTag = NAME_None;
	float MaxStaticMeshSlopeAngle = 50.f;
	bool bHasAuthority = false;
	bool bEnableDepositAccumulation = false;
	bool bDepositOnStaticMeshes = false;
	bool bBlockDepositBelowStaticMeshes = true;
	bool bTraceComplexStaticMeshSurfaces = false;
};

struct FDRVoxelTerrainOperationTickResult
{
	FDRVoxelDepositDeltaRecord DeltaRecord;
	int32 ModifiedVoxelCount = 0;
	int32 ScannedColumnCount = 0;
	int32 RemainingRequestCount = 0;
	bool bProcessedRequest = false;
	bool bChunkCompleted = false;
	bool bCancelled = false;
};

// 액터별 퇴적 패스의 가변 상태다. 알고리즘 진입점은 TerrainOperationLibrary만 공개하며,
// 이 값은 액터 멤버로 고정해 요청이 가리키는 패스 중복 방지 집합의 주소가 바뀌지 않게 한다.
struct FDRVoxelTerrainOperationState final
{
public:
	FDRVoxelTerrainOperationState() = default;
	~FDRVoxelTerrainOperationState() = default;
	FDRVoxelTerrainOperationState(const FDRVoxelTerrainOperationState&) = delete;
	FDRVoxelTerrainOperationState& operator=(const FDRVoxelTerrainOperationState&) = delete;
	FDRVoxelTerrainOperationState(FDRVoxelTerrainOperationState&&) = delete;
	FDRVoxelTerrainOperationState& operator=(FDRVoxelTerrainOperationState&&) = delete;

private:
	struct FFootprintTraceTarget
	{
		FIntPoint VoxelXY = FIntPoint::ZeroValue;
		float AmountScale = 1.f;
		float MinLocalSurfaceZ = 0.f;
		float MaxLocalSurfaceZ = 0.f;
	};

	struct FPendingFootprintTrace
	{
		FTraceHandle Handle;
		FFootprintTraceTarget Target;
	};

	bool IsActive() const;
	bool StartPass(const FDRVoxelTerrainOperationContext& Context);
	FDRVoxelTerrainOperationTickResult Tick(const FDRVoxelTerrainOperationContext& Context);
	void Cancel();
	void StartNextChunk(const FDRVoxelTerrainOperationContext& Context);
	FDRVoxelDepositInBoxRequest* GetActiveRequest(const FDRVoxelTerrainOperationContext& Context);
	FDRVoxelDepositInBoxRequest* GetActiveRequest(
		const FDRVoxelTerrainOperationContext& Context,
		EDRVoxelDepositRequestPhase ExpectedPhase);
	void ProcessRequest(
		const FDRVoxelTerrainOperationContext& Context,
		FDRVoxelTerrainOperationTickResult& OutResult);
	bool BeginSurfaceScan(
		const FDRVoxelTerrainOperationContext& Context,
		const FDRVoxelDepositInBoxSettings& RequestSettings,
		const FVector& ScanCenter,
		const FVector& ScanExtent);
	void ProcessSurfaceScan(const FDRVoxelTerrainOperationContext& Context);
	void FinishSurfaceScan(const FDRVoxelTerrainOperationContext& Context);
	void CancelSurfaceScan();
	bool BeginFootprintScan(const FDRVoxelTerrainOperationContext& Context);
	void ProcessFootprintScan(const FDRVoxelTerrainOperationContext& Context);
	void FinishFootprintScan(const FDRVoxelTerrainOperationContext& Context);
	void CancelFootprintScan();

	FDRVoxelDepositInBoxRequest ActiveRequest;
	bool bHasActiveRequest = false;
	TArray<int32> ChunkOrder;
	int32 NextChunkOrderIndex = 0;
	int32 ActiveChunkIndex = INDEX_NONE;
	int32 PassNumber = 0;
	bool bPassActive = false;
	TSet<FIntVector> PassWrittenVoxelPositions;
	TSet<FIntPoint> PassWrittenColumns;

	TArray<FTraceHandle> PendingSurfaceTraceHandles;
	TArray<FVector> SurfaceHitPositions;
	TArray<int32> SurfaceTraceColumnOrder;
	FRandomStream SurfaceTraceRandomStream;
	TWeakObjectPtr<AVoxelWorld> SurfaceScanVoxelWorld;
	FVector SurfaceScanCenter = FVector::ZeroVector;
	FVector SurfaceScanExtent = FVector::ZeroVector;
	int32 SurfaceTraceColumnCountY = 0;
	int32 NextSurfaceTraceColumnIndex = 0;
	bool bSurfaceScanActive = false;

	TArray<FPendingFootprintTrace> PendingFootprintTraces;
	TArray<FFootprintTraceTarget> FootprintTraceTargets;
	FVector FootprintScanCenter = FVector::ZeroVector;
	FVector FootprintScanExtent = FVector::ZeroVector;
	int32 NextFootprintTraceIndex = 0;
	bool bFootprintScanActive = false;

	friend class UDRVoxelTerrainOperationLibrary;
};

UCLASS()
class DEEPRAIDERS_API UDRVoxelTerrainOperationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 액터가 소유한 상태를 통해 전체 퇴적 패스를 시작·진행·취소하는 유일한 공개 진입점이다.
	static bool IsDepositPassActive(const FDRVoxelTerrainOperationState& State);
	static bool StartDepositPass(
		const FDRVoxelTerrainOperationContext& Context,
		FDRVoxelTerrainOperationState& State);
	static FDRVoxelTerrainOperationTickResult TickDeposit(
		const FDRVoxelTerrainOperationContext& Context,
		FDRVoxelTerrainOperationState& State);
	static void CancelDeposit(FDRVoxelTerrainOperationState& State);

	// 월드 공간 구와 축 정렬 박스가 겹치는지 판정한다. 굴착이 이 액터의 관리 영역에 영향을 주는지 거르는 용도다.
	UFUNCTION(BlueprintPure, Category="Voxel Terrain|Query")
	static bool IsVoxelUpdateInBox(
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FVector& Location,
		float Radius);

	// 입력을 검증하고 월드 영역을 로컬 복셀 좌표로 변환한 뒤, 틱 분할 처리용 요청 상태를 만든다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool MakeDepositInBoxRequest(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FDRVoxelDepositInBoxSettings& Settings,
		FDRVoxelDepositInBoxRequest& OutRequest);

	// 청크 요청의 쓰기 범위를 퍼짐 반경만큼 확장하되 전체 관리 박스 밖으로는 나가지 않게 제한한다.
	// Blueprint 옵션이 아니라 청크 스케줄러가 요청 생성 직후 호출하는 C++용 경계 구성 함수다.
	static bool ConfigureDepositRequestWriteBounds(
		FDRVoxelDepositInBoxRequest& Request,
		const FVector& AreaCenter,
		const FVector& AreaExtent);

	// 비동기 물리 트레이스로 찾은 고정 메시 표면을 기존 복셀 퇴적 요청에 합친다.
	// 여기서는 커버리지 선택용 중심만 추가한다. 선택된 중심 주변의 실제 메시 높이와 가장자리는
	// 액터의 두 번째 비동기 트레이스 단계에서 확정하고, 최종 기록은 일반 후보와 같은 경로를 사용한다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool AddExternalSurfaceDepositCandidates(
		UPARAM(ref) FDRVoxelDepositInBoxRequest& Request,
		const TArray<FVector>& SurfaceWorldPositions,
		int32& OutAddedCandidateCount);

	// 단일 요청의 현재 Phase 하나를 틱 예산만큼 진행한다.
	// 반환값은 처리 성공 여부가 아니라 다음 틱에도 같은 요청을 계속 처리해야 하는지를 의미한다.
	// Out 값은 매 호출마다 초기화되며, ApplyVoxels에서 실제 변경이 생긴 Tick에만 DeltaRecord가 채워진다.
	// 지형 작업 상태는 동시에 요청 하나만 소유하므로 이 API를 사용해 불필요한 배열 큐 상태를 만들지 않는다.
	static bool ProcessDepositInBoxRequestTick(
		FDRVoxelDepositInBoxRequest& Request,
		int32 MaxScanColumnsToProcess,
		int32 MaxVoxelWriteAttemptsToProcess,
		int32& OutModifiedVoxelCount,
		int32& OutScannedColumnCount,
		FDRVoxelDepositDeltaRecord& OutDeltaRecord);

	// 배열의 첫 요청을 틱 예산만큼 진행하고, 완료된 요청은 배열에서 제거한다.
	// 실제 변경이 생긴 틱에만 OutDeltaRecord가 채워져 서버가 해당 결과를 복제할 수 있다.
	// 반환값은 성공 여부가 아니라 처리 후 Requests에 요청이 남아 있는지를 의미한다.
	// Blueprint 및 기존 C++ 호출자를 위한 호환 API이며 내부 처리는 단일 요청 API에 위임한다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool ProcessDepositInBoxRequestsTick(
		UPARAM(ref) TArray<FDRVoxelDepositInBoxRequest>& Requests,
		int32 MaxScanColumnsToProcess,
		int32 MaxVoxelWriteAttemptsToProcess,
		int32& OutModifiedVoxelCount,
		int32& OutScannedColumnCount,
		FDRVoxelDepositDeltaRecord& OutDeltaRecord,
		int32& OutRemainingRequestCount);

	// 서버가 보낸 압축 델타를 클라이언트 복셀 월드에 일괄 재생한다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool ApplyDepositDeltaRecord(
		AVoxelWorld* VoxelWorld,
		const FDRVoxelDepositDeltaRecord& DeltaRecord,
		int32& OutAppliedVoxelCount);

	// 서버에서 발생한 구형 굴착을 클라이언트 또는 중도 난입 클라이언트에 재생한다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Dig")
	static bool ApplyDigDeltaRecord(
		AVoxelWorld* VoxelWorld,
		const FDRVoxelDigDeltaRecord& DeltaRecord);
};
