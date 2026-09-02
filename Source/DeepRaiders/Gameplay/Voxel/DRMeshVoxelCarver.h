#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRMeshVoxelCarver.generated.h"

class AVoxelWorld;
class UStaticMeshComponent;

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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	TObjectPtr<UStaticMeshComponent> CarveMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Voxel Carver")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Carver")
	bool bCarveOnBeginPlay = true;

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

private:
	AVoxelWorld* ResolveVoxelWorld();
	bool StartCarveAsync(TFunction<void()>&& Completion);
	void TryExecuteCarveBatch();
	void ExecuteNextCarver();

	FTimerHandle RetryTimerHandle;
	int32 RetryCount = 0;
	int32 PendingCarverIndex = 0;
	TArray<TWeakObjectPtr<ADRMeshVoxelCarver>> PendingCarvers;
};
