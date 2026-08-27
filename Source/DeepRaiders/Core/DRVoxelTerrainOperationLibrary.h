#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldCollision.h"
#include "DRVoxelTerrainOperationLibrary.generated.h"

class AActor;
class AVoxelWorld;
class UWorld;
class UDRVoxelTerrainOperationLibrary;

// 지형 조작에서 공유하는 좌표 계산 규칙이다.
namespace DRVoxelTerrain
{
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

	template<typename ElementType>
	void ShuffleArray(TArray<ElementType>& Values, FRandomStream& RandomStream)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			Values.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}

	// 전체 후보 배열을 만들지 않고 PopulationSize에서 중복 없는 인덱스만 골라낸다.
	// Floyd 표본 추출을 사용하므로 관리 영역이 넓어도 준비 비용은 RequestedCount에 비례한다.
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
	// 요청 생성 시 무작위로 고른 X/Y 열에서 지표면 위의 퇴적 후보를 수집하는 단계다.
	BuildCandidates,
	// 선택된 중심의 원형 풋프린트 각 칸에서 실제 표면 높이를 다시 찾아 최종 쓰기 위치를 만드는 단계다.
	ResolveFootprints,
	// 표면 해석이 끝난 위치에 복셀 값과 머터리얼을 기록하는 단계다.
	ApplyVoxels,
	// 모든 샘플 열과 남은 후보를 처리해 요청 배열에서 제거해도 되는 상태다.
	Finished
};

// 한 번의 무작위 표면 퇴적 요청에서 실제로 조절할 값만 모은 설정이다.
// 지터, 가장자리 감쇠, 최상단 표면 전용 처리 같은 구현 세부값은 내부 기본값으로 고정한다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxSettings
{
	GENERATED_BODY()

	// 무작위 표본을 고를 X/Y 격자의 월드 단위 간격이다. 최소 표본 간격과 위치 분포를 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float SurfaceSampleSpacing = 50.f;

	// 이번 요청에서 선택된 위치의 밀도값에서 뺄 양이다. 값이 클수록 한 번에 두껍게 쌓인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositAmountPerPass = 0.05f;

	// 새로 쌓인 복셀에 기록할 단일 머터리얼 인덱스다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	// 한 요청에서 무작위로 검사할 X/Y 열의 최대 개수다.
	// 관리 영역 크기와 무관하게 검사량이 이 값으로 제한되며 각 열은 전체 Z를 위에서 아래로 조사한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="1"))
	int32 RandomSurfaceSampleCount = 16;

	// 선택된 표면 하나가 주변으로 퍼지는 월드 단위 반경이다. 요청 생성 시 VoxelSize 기준 정수 반경으로 변환된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositSpreadRadius = 100.f;

	// 서버가 요청마다 정하고 RPC로 함께 보내는 결정적 난수 Seed다.
	// 서버와 클라이언트는 같은 Seed로 동일한 표본 위치와 적용 순서를 계산한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 RandomSeed = 0;
};

// 한 번의 적설을 재현하는 작은 고수준 명령이다.
// 변경된 복셀 배열을 네트워크로 보내지 않고 이 값들만 RPC로 보내므로 영역이 커져도 RPC 크기가 일정하다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositCommand
{
	GENERATED_BODY()

	// 이번 요청에서 무작위 표면 표본을 고를 월드 공간 박스다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FVector ScanCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FVector ScanExtent = FVector::ZeroVector;

	// 원형 풋프린트가 번질 수 있는 전체 관리 영역이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FVector AreaCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FVector AreaExtent = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FDRVoxelDepositInBoxSettings Settings;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit|Static Mesh")
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit|Static Mesh")
	float MaxStaticMeshSlopeAngle = 50.f;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit|Static Mesh")
	bool bDepositOnStaticMeshes = false;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit|Static Mesh")
	bool bBlockDepositBelowStaticMeshes = true;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit|Static Mesh")
	bool bTraceComplexStaticMeshSurfaces = false;
};

// 한 번의 동기 호출 안에서 사용하는 퇴적 작업 자료다.
// 요청 생성 시 고른 제한된 수의 무작위 열과 해당 풋프린트만 처리한다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxRequest
{
	GENERATED_BODY()

	// 이 요청이 읽고 쓸 대상 월드다. 동기 처리 시작 시 유효성을 검사한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	// 관리 액터의 전체 검사 범위다. 실제로 조사할 X/Y 열은 요청 생성 시 이 범위에서 무작위로 제한한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMax = FIntVector::ZeroValue;

	// 선택된 중심의 원형 풋프린트가 실제로 값을 기록할 수 있는 범위다.
	// 관리 액터 영역 밖으로 눈이 번지지 않도록 별도로 보관한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector WriteVoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector WriteVoxelMax = FIntVector::ZeroValue;

	// Build 단계에서 무작위 표본 열에서 발견한 후보를 담고 Resolve 단계에서 풋프린트 중심으로 사용한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TArray<FIntVector> PendingVoxels;

	// 섞인 ScanColumnOrder의 현재 항목을 실제 로컬 X/Y 좌표로 변환한 결과다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntPoint ScanCursor = FIntPoint::ZeroValue;

	// SurfaceSampleSpacing을 VoxelSize로 나눈 정수 복셀 간격이며 최소값은 1이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 VoxelSampleStep = 1;

	// DepositSpreadRadius를 VoxelSize로 나눈 실제 원형 풋프린트 반경이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 DepositFootprintRadius = 0;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FDRVoxelDepositInBoxSettings DepositSettings;

	// 요청 생성 검증을 통과했는지 나타낸다. false인 요청은 처리 배열에서 즉시 제거된다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bIsValid = false;

	// 동기 처리 함수가 현재 실행할 단계다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	EDRVoxelDepositRequestPhase Phase = EDRVoxelDepositRequestPhase::BuildCandidates;

	// 아래 필드는 서버가 요청을 처리하는 동안만 사용하는 런타임 상태다.
	// 최종 동기화에는 실제로 변경된 복셀만 사용하므로 이 상태 자체는 네트워크로 보내지 않는다.
	FRandomStream RandomStream;
	int32 ScanColumnCountY = 0;
	// X/Y 열의 선형 인덱스를 요청 생성 시 한 번 섞어 둔다.
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
	// 무작위 표본에서 발견한 고정 메시 후보 중심만 남긴다. 이 중심의 풋프린트는 각 칸마다
	// 동기 트레이스를 수행해 메시 가장자리와 경사를 실제 형상대로 해석한다.
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
	// 관리 액터가 활성화하면 최종 복셀 쓰기 후보 위의 WorldStatic 충돌을 검사해 천장 아래 쓰기를 막는다.
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
	const FDRVoxelDepositInBoxSettings* DepositSettings = nullptr;
	FVector AreaCenter = FVector::ZeroVector;
	FVector AreaExtent = FVector::ZeroVector;
	float RandomScanWorldSize = 2000.f;
	FName RequiredStaticMeshSurfaceTag = NAME_None;
	float MaxStaticMeshSlopeAngle = 50.f;
	bool bDepositOnStaticMeshes = false;
	bool bBlockDepositBelowStaticMeshes = true;
	bool bTraceComplexStaticMeshSurfaces = false;
};

struct FDRVoxelTerrainOperationResult
{
	int32 ModifiedVoxelCount = 0;
	int32 ScannedColumnCount = 0;
};

UCLASS()
class DEEPRAIDERS_API UDRVoxelTerrainOperationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 액터 설정에서 RPC로 보낼 작은 고수준 명령을 만든다. 실제 지형 변경은 수행하지 않는다.
	static bool MakeDepositCommand(
		const FDRVoxelTerrainOperationContext& Context,
		FDRVoxelDepositCommand& OutCommand);

	// 명령 하나를 현재 월드에서 처음부터 끝까지 동기 실행한다.
	static bool ExecuteDepositCommand(
		UWorld* World,
		AVoxelWorld* VoxelWorld,
		AActor* TraceOwner,
		const FDRVoxelDepositCommand& Command,
		FDRVoxelTerrainOperationResult& OutResult);

	// 월드 공간 구와 축 정렬 박스가 겹치는지 판정한다. 굴착이 이 액터의 관리 영역에 영향을 주는지 거르는 용도다.
	UFUNCTION(BlueprintPure, Category="Voxel Terrain|Query")
	static bool IsVoxelUpdateInBox(
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FVector& Location,
		float Radius);

	// 입력을 검증하고 월드 영역을 로컬 복셀 좌표로 변환한 뒤 동기 처리용 요청 자료를 만든다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool MakeDepositInBoxRequest(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FDRVoxelDepositInBoxSettings& Settings,
		FDRVoxelDepositInBoxRequest& OutRequest);

	// 요청의 쓰기 범위를 전체 관리 박스 밖으로 나가지 않게 제한한다.
	// Blueprint 옵션이 아니라 관리 액터가 요청 생성 직후 호출하는 C++용 경계 구성 함수다.
	static bool ConfigureDepositRequestWriteBounds(
		FDRVoxelDepositInBoxRequest& Request,
		const FVector& AreaCenter,
		const FVector& AreaExtent);

	// 물리 트레이스로 찾은 고정 메시 표면을 기존 복셀 퇴적 요청에 합친다.
	// 여기서는 커버리지 선택용 중심만 추가한다. 선택된 중심 주변의 실제 메시 높이와 가장자리는
	// 두 번째 정밀 트레이스 단계에서 확정하고, 최종 기록은 일반 후보와 같은 경로를 사용한다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool AddExternalSurfaceDepositCandidates(
		UPARAM(ref) FDRVoxelDepositInBoxRequest& Request,
		const TArray<FVector>& SurfaceWorldPositions,
		int32& OutAddedCandidateCount);

	// 단일 요청의 현재 Phase 전체를 한 번에 처리한다. StaticMesh 정밀 트레이스 삽입을 위해
	// ExecuteDepositCommand가 Build, Resolve, Apply 순서로 호출하는 C++ 내부 단계 함수다.
	static bool ProcessDepositInBoxRequestPhase(
		FDRVoxelDepositInBoxRequest& Request,
		int32& OutModifiedVoxelCount,
		int32& OutScannedColumnCount);

	// 서버에서 발생한 구형 굴착을 현재 클라이언트에 재생한다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Dig")
	static bool ApplyDig(
		AVoxelWorld* VoxelWorld,
		const FVector& Location,
		float Radius);
};
