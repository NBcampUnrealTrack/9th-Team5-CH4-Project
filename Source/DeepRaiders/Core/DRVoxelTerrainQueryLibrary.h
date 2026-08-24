#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRVoxelTerrainQueryLibrary.generated.h"

class AVoxelWorld;

UENUM(BlueprintType)
enum class EDRVoxelDepositRequestPhase : uint8
{
	// X/Y 샘플 열을 순회하면서 지표면 위의 퇴적 후보를 수집하는 단계다.
	BuildCandidates,
	// 수집된 후보에 원형 풋프린트를 펼쳐 실제 복셀 값과 머터리얼을 기록하는 단계다.
	ApplyVoxels,
	// 모든 샘플 열과 남은 후보를 처리해 요청 배열에서 제거해도 되는 상태다.
	Finished
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

// 한 번의 퇴적 요청에서 사용하는 입력 설정이다.
// 스캔 간격/지터는 후보 위치의 분포를, 패치/풋프린트는 한 후보가 실제로 덮는 면적을 결정한다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxSettings
{
	GENERATED_BODY()

	// 월드 단위의 X/Y 샘플 간격이다. 작을수록 촘촘하고 자연스럽지만 스캔할 열이 급격히 늘어난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float SampleStep = 50.f;

	// 선택된 복셀의 밀도 값에서 한 번에 뺄 양이다. Voxel Plugin에서는 값이 작아질수록 고체가 늘어난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositAmount = 0.05f;

	// 새로 쌓인 복셀에 기록할 단일 머터리얼 인덱스다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	// 지터, 열 순서, 후보 순서를 결정한다. 같은 값이면 동일한 요청 결과를 재현할 수 있다.
	UPROPERTY(BlueprintReadWrite, Category="Voxel|Deposit")
	int32 RandomSeed = 0;

	// true면 각 X/Y 열의 가장 높은 표면 주변만 조사한다. false면 같은 열 안의 모든 표면 경계를 후보로 본다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit")
	bool bOnlyTopSurface = true;

	// 정규 격자 샘플 좌표를 무작위로 흔들어 격자무늬가 드러나는 현상을 줄인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit")
	bool bUseJitteredSamples = true;

	// VoxelSampleStep에 곱해 지터 최대 반경을 계산한다. 0은 격자 중앙, 1은 샘플 간격 전체 범위다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float JitterRatio = 0.4f;

	// 한 샘플 주변에서 높이를 비교하고 후보를 뽑는 정사각형 반경이다. 낮은 지형 선호 확률 계산 범위이기도 하다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0"))
	int32 DepositPatchRadius = 1;

	// 선택된 후보 하나가 실제로 값을 변경하려고 시도하는 원형 면적의 복셀 반경이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0"))
	int32 DepositFootprintRadius = 1;

	// 풋프린트 가장자리의 DepositAmount 배율이다. 1이면 평평하고, 0에 가까울수록 가장자리가 얇아진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FootprintEdgeStrength = 0.55f;

	// 패치에서 가장 높은 표면이 후보로 선택될 확률이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinSurfaceDepositChance = 0.15f;

	// 패치에서 가장 낮은 표면이 후보로 선택될 확률이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaxSurfaceDepositChance = 0.85f;

	// 높이 차이를 선택 확률로 바꿀 때 사용하는 지수다. 1은 선형, 클수록 아주 낮은 칸에 선택이 집중된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Deposit", meta=(ClampMin="0.01"))
	float LowerSurfaceSelectionBias = 1.5f;
};

// 여러 틱에 걸쳐 처리되는 퇴적 작업의 진행 상태다.
// 요청 생성 후 BuildCandidates와 ApplyVoxels 단계를 오가며 예산만큼 처리되고 완료되면 배열에서 제거된다.
USTRUCT(BlueprintType)
struct FDRVoxelDepositInBoxRequest
{
	GENERATED_BODY()

	// 이 요청이 읽고 쓸 대상 월드다. 처리 도중 파괴될 수 있으므로 매 틱 유효성을 다시 검사한다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	// 월드 박스를 VoxelWorld 로컬 정수 좌표로 바꾼 포함 범위다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector VoxelMax = FIntVector::ZeroValue;

	// 현재 스캔 배치에서 발견해 ApplyVoxels 단계가 처리해야 하는 후보 중심 목록이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	TArray<FIntVector> PendingVoxels;

	// PendingVoxels에서 다음으로 풋프린트를 펼칠 후보의 인덱스다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextVoxelIndex = 0;

	// 큰 풋프린트를 한 틱에 전부 쓰지 않고, 중심과 다음 오프셋을 기억해 다음 틱에서 이어서 처리한다.
	// 이 상태가 있어야 설정한 복셀 쓰기 시도 예산이 풋프린트 크기와 무관하게 지켜진다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntVector ActiveFootprintCenter = FIntVector::ZeroValue;

	// (2R+1)^2 정사각형 안에서 다음 틱에 검사할 오프셋의 선형 인덱스다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextFootprintOffsetIndex = 0;

	// true면 현재 중심의 풋프린트가 아직 끝나지 않았으므로 새 후보로 넘어가면 안 된다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	bool bHasActiveFootprint = false;

	// 섞인 ScanColumnOrder의 현재 항목을 실제 로컬 X/Y 좌표로 변환한 결과다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	FIntPoint ScanCursor = FIntPoint::ZeroValue;

	// 다음으로 처리할 ScanColumnOrder 항목이다. 배열 끝에 도달하면 후보 스캔 전체가 완료된다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 NextScanColumnIndex = 0;

	// SampleStep을 VoxelSize로 나눈 정수 복셀 간격이며 최소값은 1이다.
	UPROPERTY(BlueprintReadOnly, Category="Voxel|Deposit")
	int32 VoxelSampleStep = 1;

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
	// 후보 중복과 이미 기록한 쓰기 중복을 빠르게 제거하기 위한 서버 전용 집합이다.
	TSet<FIntVector> PendingVoxelPositions;
	TSet<FIntVector> WrittenVoxelPositions;
	// 고정 메시 표면은 복셀 밀도장에 고체로 존재하지 않는다. 키는 메시 바로 위에 쓸 복셀이고,
	// 값은 복셀 로컬 좌표계에서 측정한 실제 메시 표면 Z다. 쓰기 단계는 이 높이로 메시 안쪽의
	// 얇은 지지층과 첫 퇴적층의 밀도값을 계산해 첫 누적부터 등가면이 메시 표면에 붙도록 만든다.
	TMap<FIntVector, float> ExternalSupportSurfaceZByVoxel;
};

UCLASS()
class DEEPRAIDERS_API UDRVoxelTerrainQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 박스 안을 3차원 간격으로 샘플링하고 고체 복셀의 단일 머터리얼 인덱스별 개수를 센다.
	// TargetMaterialIndices가 비어 있으면 모든 머터리얼을 집계한다.
	UFUNCTION(BlueprintCallable, Category="Voxel Terrain|Query")
	static bool GetMaterialCountsInBox(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		float SampleStep,
		const TArray<uint8>& TargetMaterialIndices,
		TMap<uint8, int32>& OutMaterialCounts,
		int32& OutTotalCount);

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

	// 비동기 물리 트레이스로 찾은 고정 메시 표면을 기존 복셀 퇴적 요청에 합친다.
	// 각 표면은 복셀 좌표로 변환되고 풋프린트 범위까지 외부 지지 높이가 기록되며,
	// 실제 값/머터리얼 변경은 일반 후보와 동일하게 ProcessDepositInBoxRequestsTick에서 수행된다.
	UFUNCTION(BlueprintCallable, Category="Voxel|Deposit")
	static bool AddExternalSurfaceDepositCandidates(
		UPARAM(ref) FDRVoxelDepositInBoxRequest& Request,
		const TArray<FVector>& SurfaceWorldPositions,
		int32& OutAddedCandidateCount);

	// 배열의 첫 요청을 틱 예산만큼 진행하고, 완료된 요청은 배열에서 제거한다.
	// 실제 변경이 생긴 틱에만 OutDeltaRecord가 채워져 서버가 해당 결과를 복제할 수 있다.
	// 반환값은 성공 여부가 아니라 처리 후 Requests에 요청이 남아 있는지를 의미한다.
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
