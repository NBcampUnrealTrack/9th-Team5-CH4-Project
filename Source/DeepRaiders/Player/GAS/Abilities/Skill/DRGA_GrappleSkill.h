#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DeepRaiders/Combat/Grapple/DRGrappleAbilityTypes.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_GrappleSkill.generated.h"

class ADRGrappleTargetActor;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UDRSkillDefinition;
struct FGameplayEventData;

enum class EDRGrappleSkillPhase : uint8
{
	Inactive,
	Targeting,
	HookFlying,
	Grappling,
	Ending,
};

/** SkillDefinition을 비용과 쿨다운 출처로 사용하는 그래플 이동 스킬. */
UCLASS()
class DEEPRAIDERS_API UDRGA_GrappleSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_GrappleSkill();

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	enum class EDRTargetValidationResult : uint8
	{
		InvalidRequest,
		Failed,
		Succeeded,
	};

	void StartTargeting();
	void StartCancelEventTask();
	bool BeginHookFlight(const FVector& InHookLocation, const FVector& InHookNormal);
	float CalculateHookFlightDuration(const FVector& InHookLocation) const;
	void HandleHookFlightFinished();

	EDRTargetValidationResult ValidateServerTargetData(
		const FGameplayAbilityTargetDataHandle& TargetData,
		FVector& OutTargetLocation,
		FVector& OutTargetNormal) const;

	FDRMovementActionState BuildMovementActionState(const FVector& InHookLocation) const;
	bool StartPredictedMovement();
	bool StartAuthoritativeMovement();
	void ApplyMovementActionTag();
	void RemoveMovementActionTag();
	void QueueEndGrapple(EDRMovementActionEndReason EndReason);
	void ApplyQueuedGrappleEnd();
	void StopMovementAction(EDRMovementActionEndReason EndReason);
	float GetMovementElapsedTime() const;
	int32 ResolveSessionId() const;
	FVector ResolveViewDirection() const;

	void StartGrappleGameplayCue(const FVector& InHookLocation, const FVector& InHookNormal);
	void StopGrappleGameplayCue();
	void PlayFailedGrappleGameplayCue(const FVector& FailedLocation);

	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleCancelEventReceived(FGameplayEventData Payload);

	void HandleMovementActionEnded(EDRMovementActionEndReason EndReason);
	void HandleMovementActionSimulated(const FDRMovementActionSimulationResult& Result);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple", meta = (ShowOnlyInnerProperties))
	FDRGrappleAbilitySettings GrappleSettings;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Grapple")
	TSubclassOf<ADRGrappleTargetActor> TargetActorClass;

	UPROPERTY(Transient)
	TObjectPtr<UDRSkillDefinition> ActiveSkillDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelEventTask;

	FVector HookLocation = FVector::ZeroVector;
	FVector HookSurfaceNormal = FVector::ZeroVector;
	float MovementStartTimeSeconds = -1.f;
	float HookFlightDuration = 0.f;

	FTimerHandle EndGrappleTimerHandle;
	FTimerHandle HookFlightTimerHandle;

	EDRGrappleSkillPhase GrapplePhase = EDRGrappleSkillPhase::Inactive;
	EDRMovementActionEndReason PendingEndReason = EDRMovementActionEndReason::Invalidated;

	bool bMovementStarted = false;
	bool bMovementActionTagApplied = false;
	bool bEndQueued = false;
	bool bEndingGrapple = false;
	bool bGrappleGameplayCueActive = false;
};
