#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/Combat/Grapple/DRGrappleAbilityTypes.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DRGA_GrappleItem.generated.h"

class ADRGrappleTargetActor;
class UAbilityTask_WaitInputPress;
class UAbilityTask_WaitTargetData;
class UDRInventoryComponent;
class UDRItemDefinition;

UCLASS()
class DEEPRAIDERS_API UDRGA_GrappleItem : public UGameplayAbility
{
	GENERATED_BODY()

public:
    UDRGA_GrappleItem();

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
    bool ResolveSelectedItem(
        const FGameplayAbilityActorInfo* ActorInfo,
        const UDRItemDefinition* ExpectedDefinition,
        UDRInventoryComponent*& OutInventory,
        FGuid& OutInstanceId) const;

    int32 ResolveInputId(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo) const;

    int32 ResolveSessionId() const;

    void StartTargeting();
    void StartCancelInputTask();

    bool ValidateServerTargetData(
        const FGameplayAbilityTargetDataHandle& TargetData,
        FVector& OutHookLocation) const;

    FDRMovementActionState BuildMovementActionState(
        const FVector& HookLocation) const;

    bool StartPredictedMovement(
        const FVector& HookLocation);

    bool StartAuthoritativeMovement(
        const FVector& HookLocation);

    void ApplyMovementActionTag();
    void RemoveMovementActionTag();

    void QueueEndGrapple(
        EDRMovementActionEndReason EndReason);

    void ApplyQueuedGrappleEnd();

    void StopMovementAction(
        EDRMovementActionEndReason EndReason);

    UFUNCTION()
    void HandleTargetDataReady(
        const FGameplayAbilityTargetDataHandle& TargetData);

    UFUNCTION()
    void HandleTargetDataCanceled(
        const FGameplayAbilityTargetDataHandle& TargetData);

    UFUNCTION()
    void HandleCancelInputPressed(
        float TimeWaited);

    void HandleMovementActionEnded(
        EDRMovementActionEndReason EndReason);

    void HandleMovementActionSimulated(
        const FDRMovementActionSimulationResult& Result);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple", meta = (ShowOnlyInnerProperties))
    FDRGrappleAbilitySettings GrappleSettings;
    
private:
    UPROPERTY(EditDefaultsOnly, Category = "Grapple")
    TSubclassOf<ADRGrappleTargetActor> TargetActorClass;

    UPROPERTY(Transient)
    TObjectPtr<UDRItemDefinition> ActiveItemDefinition;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitInputPress> CancelInputTask;

    FGuid ActiveInstanceId;
    FVector HookLocation = FVector::ZeroVector;

    FTimerHandle EndGrappleTimerHandle;

    EDRMovementActionEndReason PendingEndReason = EDRMovementActionEndReason::Invalidated;

    bool bMovementStarted = false;
    bool bMovementActionTagApplied = false;
    bool bEndQueued = false;
    bool bEndingGrapple = false;
};
