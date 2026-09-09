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

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	static constexpr int32 ContainmentLayerCount = 3;
	static constexpr int32 ContainmentSamplesPerLayer = 5;
	static constexpr int32 EscapeProbeDirectionCount = 9;
	static constexpr int32 EscapeProbeMaxStepCount = 8;

	struct FVoxelCapsuleOccupancy
	{
		int32 FullySurroundedLayerCount = 0;
		uint8 SolidLayerMasks[ContainmentLayerCount] = {};
		FVector SampleLocations[ContainmentLayerCount][ContainmentSamplesPerLayer] = {};
		bool bSampleInsideWorld[ContainmentLayerCount][ContainmentSamplesPerLayer] = {};
	};

	struct FCapsuleEscapeProbeResult
	{
		bool bValid = false;
		bool bStartFits = false;
		bool bEscapePathFound = false;
		int32 EscapeDirectionIndex = INDEX_NONE;
		float StartOccupancyScore = 0.f;
		float BestEndOccupancyScore = 1.f;
		FVector StepLocations[EscapeProbeDirectionCount][EscapeProbeMaxStepCount] = {};
		float StepOccupancyScores[EscapeProbeDirectionCount][EscapeProbeMaxStepCount] = {};
		bool bStepFits[EscapeProbeDirectionCount][EscapeProbeMaxStepCount] = {};
		bool bStepPathConnected[EscapeProbeDirectionCount][EscapeProbeMaxStepCount] = {};
	};

	UAbilitySystemComponent* GetAbilitySystemComponent() const;
	FVoxelCapsuleOccupancy GetVoxelCapsuleOccupancy(AVoxelWorld& VoxelWorld) const;
	void EnterVoxelContainedMode(AVoxelWorld& VoxelWorld);
	void UpdateVoxelContainedMode();
	void ApplyFreezeGain();
	void StartContainmentTimers();
	void StopContainmentTimers();
	void ClearContainmentState();
	AVoxelWorld* ResolveDebugVoxelWorld();
	FCapsuleEscapeProbeResult GetCapsuleEscapeProbe(AVoxelWorld& VoxelWorld) const;
	void DrawContainmentDebug(AVoxelWorld& VoxelWorld) const;

	TWeakObjectPtr<AVoxelWorld> VoxelContainmentWorld;
	TWeakObjectPtr<AVoxelWorld> DebugVoxelWorld;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	FActiveGameplayEffectHandle ContainmentEffectHandle;

	/** 중심과 사방이 동시에 고체인 수평 단면이 이 개수 이상일 때만 매몰로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "3"))
	int32 RequiredSurroundedLayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ReleaseDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float CheckInterval = 0.2f;

	/** 최대 HP가 3초에 차도록 적용되는 초당 빙결량. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Freeze", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float FreezeDeathDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Freeze", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float FreezeTickInterval = 0.1f;

	/** Data.Freeze.Amount SetByCaller를 받는 Instant GameplayEffect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Freeze", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> FreezeGainEffectClass;

	/** 개발 빌드에서 매몰 판정 표본과 최종 결과를 월드에 계속 표시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDrawContainmentDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float DebugDrawInterval = 0.05f;

	/** 전후좌우, 대각선, 위쪽으로 이어진 캡슐 이동 경로를 검사할 단계 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Escape", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "8"))
	int32 EscapeProbeStepCount = 6;

	/** 각 탈출 경로 검사 단계의 간격. 캡슐 반지름에 곱해 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Escape", meta = (AllowPrivateAccess = "true", ClampMin = "0.25", ClampMax = "2.0"))
	float EscapeProbeStepDistanceScale = 0.35f;

	/** 캡슐 표본의 고체 점유도가 이 값 이하면 열린 공간으로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Escape", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float EscapeProbeOpenScore = 0.05f;

	/** 경로가 고체를 뚫고 지나가지 않도록 단계 사이에서 허용할 점유도 증가량. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Containment|Escape", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "0.25"))
	float EscapeProbeAllowedScoreIncrease = 0.03f;

	float ReleaseStartTime = -1.f;
	float DebugDrawElapsedTime = 0.f;
	FTimerHandle ContainmentCheckTimerHandle;
	FTimerHandle FreezeGainTimerHandle;
};
