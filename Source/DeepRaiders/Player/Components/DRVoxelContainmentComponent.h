#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRVoxelContainmentComponent.generated.h"

class AVoxelWorld;
class UDRCharacterMovementComponent;

/**
 * 복셀 편집으로 캡슐의 내부 공간이 실제로 사라졌는지 판정하고,
 * 매몰 상태의 유지 및 해제를 관리한다.
 *
 * 이동 물리 자체는 UDRCharacterMovementComponent의 VoxelContained
 * custom movement mode가 처리한다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRVoxelContainmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRVoxelContainmentComponent();

	/** 복셀 편집 완료 후 collision 갱신 직전에 호출한다. */
	void EvaluateVoxelContainment(AVoxelWorld* VoxelWorld);

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FVoxelCapsuleOccupancy
	{
		int32 FullySurroundedLayerCount = 0;
	};

	UDRCharacterMovementComponent* GetMovementComponent() const;
	FVoxelCapsuleOccupancy GetVoxelCapsuleOccupancy(AVoxelWorld& VoxelWorld) const;
	void EnterVoxelContainedMode(AVoxelWorld& VoxelWorld);
	void UpdateVoxelContainedMode();
	void ClearContainmentState();

	TWeakObjectPtr<AVoxelWorld> VoxelContainmentWorld;

	/** 중심과 사방이 동시에 고체인 수평 단면이 이 개수 이상일 때만 매몰로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "3"))
	int32 RequiredSurroundedLayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ReleaseDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float CheckInterval = 0.05f;

	float NextCheckTime = 0.f;
	float ReleaseStartTime = -1.f;
};
