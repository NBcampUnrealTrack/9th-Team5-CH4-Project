#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HAL/ThreadSafeBool.h"
#include "VoxelIntBox.h"
#include "DRMeshVoxelCarver.generated.h"

class AVoxelWorld;
class ADRMiningGameStateBase;
class UStaticMeshComponent;

// 같은 복셀 마스크를 완성 판정과 종료 정리에 사용한다.
struct FDRMeshVoxelMask
{
	FVoxelIntBox Bounds;
	TSet<FIntVector> InsideVoxels;
};

UENUM(BlueprintType)
enum class EDRMeshVoxelCarveMode : uint8
{
	Remove,
	Add
};

UENUM(BlueprintType)
enum class EDRMeshVoxelSampleShape : uint8
{
	Cube,
	Sphere
};

/** Static Mesh의 닫힌 표면 내부에 복셀을 생성하거나 제거한다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRMeshVoxelCarver : public AActor
{
	GENERATED_BODY()

public:
	ADRMeshVoxelCarver();

	UFUNCTION(BlueprintCallable, Category = "Voxel Carver")
	bool CarveVoxelWorld();

	/** 복셀 월드 초기화 후 배치 carve를 다시 시작한다. */
	void RestartCarveBatch();
	bool ShouldCarveOnGameStart(int32 PhaseIndex) const;
	bool IsCarving() const;

	static bool BuildMeshVoxelMask(UStaticMeshComponent* Mesh, AVoxelWorld* VoxelWorld,
		int32 MaxSamples, FDRMeshVoxelMask& OutMask);
	/** 메쉬 데이터를 복사한 뒤 작업 스레드에서 마스크를 생성한다. 콜백은 게임 스레드다. */
	static bool BuildMeshVoxelMasksAsync(AVoxelWorld* VoxelWorld,
		const TArray<TPair<UStaticMeshComponent*, int32>>& Meshes,
		const TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe>& Cancellation,
		TFunction<void(TArray<FDRMeshVoxelMask>&&)>&& Completion);
	/** 비동기로 Box 안의 비보존 복셀을 제거하고 FillMask 내부의 빈 복셀을 채운다. */
	static bool TrimOutsideMesh(AVoxelWorld* VoxelWorld, const FTransform& BoxTransform,
		const FVector& BoxExtent, const FDRMeshVoxelMask& KeepMask, int32 MaxSamples,
		const FDRMeshVoxelMask& FillMask,
		const TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe>& Cancellation,
		TFunction<void(bool)>&& Completion);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	TObjectPtr<UStaticMeshComponent> CarveMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Voxel Carver")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	bool bCarveOnBeginPlay = true;

	/** 게임 시작을 위한 복셀 월드 초기화 후 다시 Carve할지 결정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	bool bCarveOnGameStart = true;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel Carver",
		meta = (ClampMin = "0", EditCondition = "bCarveOnGameStart"))
	int32 StartPhaseIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	EDRMeshVoxelCarveMode CarveMode = EDRMeshVoxelCarveMode::Remove;

	// 낮은 Priority부터 적용되므로 높은 Priority의 결과가 최종 상태를 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	int32 Priority = 0;

	// 값이 클수록 빠르고 결과가 거칠어진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver",
		meta = (ClampMin = "1", UIMax = "16"))
	int32 SamplingStep = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	EDRMeshVoxelSampleShape SampleShape = EDRMeshVoxelSampleShape::Cube;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver",
		meta = (ClampMin = "0.5", UIMax = "1.0",
			EditCondition = "SampleShape == EDRMeshVoxelSampleShape::Sphere"))
	float SphereOverlap = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver",
		meta = (ClampMin = "0.0", UIMax = "1.0",
			EditCondition = "SampleShape == EDRMeshVoxelSampleShape::Sphere"))
	float RandomOffsetRatio = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver",
		meta = (ClampMin = "0.0", UIMax = "0.75",
			EditCondition = "SampleShape == EDRMeshVoxelSampleShape::Sphere"))
	float SphereNoiseStrength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver",
		meta = (ClampMin = "0.01", UIMax = "1.0",
			EditCondition = "SampleShape == EDRMeshVoxelSampleShape::Sphere"))
	float SphereNoiseScale = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver",
		meta = (EditCondition = "SampleShape == EDRMeshVoxelSampleShape::Sphere"))
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	bool bHideMeshAfterCarve = true;

	// 내부 판정을 수행할 최대 샘플 개수다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver", meta = (ClampMin = "1"))
	int32 MaxVoxelCount = 2000000;

	// 한 번의 비동기 작업에서 처리할 Voxel 축 길이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver", meta = (ClampMin = "8"))
	int32 CarveChunkSize = 64;

	// Mesh Bounds의 겉쪽 Chunk부터 중심 방향으로 처리한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	bool bCarveOutsideIn = true;

private:
	AVoxelWorld* ResolveVoxelWorld();
	void StartCarveBatch(bool bForGameStart);
	bool StartCarveAsync(TFunction<void(bool)>&& Completion);
	void TryExecuteCarveBatch();
	void ExecuteNextCarver();
	void ClearWorldReadyBindings();
	void HandleWorldReadyTimeout();

	UFUNCTION()
	void HandleVoxelWorldGenerated();

	TArray<TWeakObjectPtr<AVoxelWorld>> WaitingVoxelWorlds;
	FTimerHandle WorldReadyTimeoutHandle;

	UFUNCTION()
	void HandleGamePhaseChanged(
		int32 PhaseIndex,
		int32 PhaseRemainingSeconds,
		const TArray<FText>& PlayerMessages);

	FTimerHandle RetryTimerHandle;
	TWeakObjectPtr<ADRMiningGameStateBase> MiningGameState;
	bool bCarveBatchForGameStart = false;
	bool bStartedForCurrentGame = false;
	int32 ActiveGamePhaseIndex = INDEX_NONE;
	int32 RetryCount = 0;
	int32 PendingCarverIndex = 0;
	TArray<TWeakObjectPtr<ADRMeshVoxelCarver>> PendingCarvers;
	bool bBatchSucceeded = true;
	uint32 BatchGeneration = 0;
	TSharedPtr<FThreadSafeBool, ESPMode::ThreadSafe> CarveCancellation;
};
