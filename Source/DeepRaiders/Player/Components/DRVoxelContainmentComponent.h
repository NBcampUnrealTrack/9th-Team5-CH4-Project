#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "DRVoxelContainmentComponent.generated.h"

class AVoxelWorld;
class UAbilitySystemComponent;
class UGameplayEffect;

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

	void BindAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FVoxelCapsuleOccupancy
	{
		int32 FullySurroundedLayerCount = 0;
	};

	UAbilitySystemComponent* GetAbilitySystemComponent() const;
	FVoxelCapsuleOccupancy GetVoxelCapsuleOccupancy(AVoxelWorld& VoxelWorld) const;
	void EnterVoxelContainedMode(AVoxelWorld& VoxelWorld);
	void UpdateVoxelContainedMode();
	void ApplyFreezeGain();
	void StartContainmentTimers();
	void StopContainmentTimers();
	void ClearContainmentState();

	TWeakObjectPtr<AVoxelWorld> VoxelContainmentWorld;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	FActiveGameplayEffectHandle ContainmentEffectHandle;

	/** 중심과 사방이 동시에 고체인 수평 단면이 이 개수 이상일 때만 매몰로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "3"))
	int32 RequiredSurroundedLayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ReleaseDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float CheckInterval = 0.05f;

	/** 최대 HP가 3초에 차도록 적용되는 초당 빙결량. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Freeze", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float FreezeDeathDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Freeze", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float FreezeTickInterval = 0.1f;

	/** Data.Freeze.Amount SetByCaller를 받는 Instant GameplayEffect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Freeze", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> FreezeGainEffectClass;

	float ReleaseStartTime = -1.f;
	FTimerHandle ContainmentCheckTimerHandle;
	FTimerHandle FreezeGainTimerHandle;
};
