#pragma once

#include "CoreMinimal.h"
#include "DRVoxelDepositOperations.generated.h"

class AActor;
class AVoxelWorld;
class UWorld;
class UPrimitiveComponent;

/** 퇴적을 허용할 월드 축 기준 영역 모양입니다. */
UENUM(BlueprintType)
enum class EDRVoxelDepositAreaShape : uint8
{
	Box,
	Sphere,
	Cylinder
};


USTRUCT()
struct FDRVoxelDepositSettings
{
	GENERATED_BODY()

	UPROPERTY()
	int32 DropsPerInterval = 16;

	/** 한 회당 표면 상승량(복셀 단위). 평탄화 보정 후에도 최대 0.25셀만 상승합니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositAmountPerPass = 0.05f;

	/** 현재 퇴적에 적용될 복셀 머터리얼 인덱스입니다. (bUseTeamId에 따라 자동 설정되거나 수동 지정됩니다) */
	UPROPERTY(VisibleAnywhere, Category="Voxel|Deposit")
	uint8 DepositMaterialIndex = 0;

	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0"))
	float DepositSpreadRadius = 20.f;

	/** 양옆 표면보다 낮으면 더 쌓고 높으면 덜 쌓습니다. 0이면 기존 퇴적량입니다. */
	UPROPERTY(EditAnywhere, Category="Voxel|Deposit", meta=(ClampMin="0.0", ClampMax="2.0"))
	float LevelingStrength = 1.f;

	UPROPERTY()
	int32 RandomSeed = 0;
};

USTRUCT()
struct FDRVoxelDepositCommand
{
	GENERATED_BODY()

	UPROPERTY()
	FVector AreaCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector AreaExtent = FVector::ZeroVector;

	UPROPERTY()
	EDRVoxelDepositAreaShape AreaShape = EDRVoxelDepositAreaShape::Box;

	/** Extent는 박스 반크기 또는 (반지름, 반지름, 절반 높이)입니다. */
	bool ContainsWorldPosition(const FVector& Position) const;

	UPROPERTY()
	FDRVoxelDepositSettings Settings;

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
	bool bAllowBelowSupport = false;
	// 서로 다른 메시 지지면을 평탄화 이웃으로 섞지 않습니다. 서버 준비 단계 전용입니다.
	TWeakObjectPtr<UPrimitiveComponent> SupportComponent;
};

/** 서버에서 실제로 기록한 값입니다. 같은 pass의 모든 셀은 같은 머터리얼을 씁니다. */
USTRUCT()
struct FDRVoxelDepositCell
{
	GENERATED_BODY()

	UPROPERTY()
	FIntVector Position = FIntVector::ZeroValue;

	UPROPERTY()
	int16 Value = 0;

	// 기존 고체의 값만 조절할 때는 팀 머터리얼을 보존합니다.
	UPROPERTY()
	bool bPaintMaterial = true;
};

USTRUCT()
struct FDRVoxelDepositResult
{
	GENERATED_BODY()

	// 큰 pass는 기존 시퀀스 큐에서 나눠 보냅니다.
	static constexpr int32 MaxCellsPerRecord = 128;

	UPROPERTY()
	FName VoxelWorldName = NAME_None;

	UPROPERTY()
	uint8 MaterialIndex = 0;

	UPROPERTY()
	TArray<FDRVoxelDepositCell> Cells;
};

struct FDRVoxelDepositPlan
{
	FDRVoxelDepositSettings Settings;
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
	 * 서버 표면을 검사하고 즉시 적용할 퇴적 계획을 만듭니다.
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
	 * @param OutResult 서버가 실제로 기록한 결과입니다.
	 * @return 계획 적용 또는 빈 계획 처리가 성공하면 true입니다.
	 */
	static bool ApplyDepositPlan(
		AVoxelWorld* VoxelWorld,
		FDRVoxelDepositPlan& Plan,
		FDRVoxelDepositResult& OutResult);

	/** 클라이언트는 표면을 재검사하지 않고 서버의 최종 값을 기록합니다. */
	static bool ApplyDepositResult(AVoxelWorld* VoxelWorld, const FDRVoxelDepositResult& Result);

	/** 연결된 양옆 표본이 있는 축만 사용해 평면 경사를 유지합니다. */
	static void LevelDepositPlan(AVoxelWorld* VoxelWorld, FDRVoxelDepositPlan& Plan);

};
