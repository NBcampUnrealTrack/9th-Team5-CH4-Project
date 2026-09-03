#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/Combat/Grapple/DRGrappleAbilityTypes.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DRGA_GrappleItem.generated.h"

class ADRGrappleTargetActor;
class UAbilityTask_WaitTargetData;
class UDRInventoryComponent;
class UDRItemDefinition;
class UAbilityTask_WaitGameplayEvent;
struct FGameplayEventData;

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
    enum class EDRGrappleTargetValidationResult : uint8
    {
        InvalidRequest,
        Failed,
        Succeeded,
    };
    
    bool ResolveSelectedItem(const FGameplayAbilityActorInfo* ActorInfo, const UDRItemDefinition* ExpectedDefinition,
        UDRInventoryComponent*& OutInventory, FGuid& OutInstanceId) const;

    int32 ResolveInputId(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

    int32 ResolveSessionId() const;

    FVector ResolveViewDirection() const;
    
    void StartTargeting();
    void StartCancelEventTask();

    EDRGrappleTargetValidationResult ValidateServerTargetData(
        const FGameplayAbilityTargetDataHandle& TargetData,
        FVector& OutTargetLocation,
        FVector& OutTargetNormal) const;

    FDRMovementActionState BuildMovementActionState(const FVector& InHookLocation) const;

    bool StartPredictedMovement(const FVector& HookLocation, const FVector& HookNormal);

    bool StartAuthoritativeMovement(const FVector& HookLocation, const FVector& HookNormal);

    void ApplyMovementActionTag();
    void RemoveMovementActionTag();

    void QueueEndGrapple(EDRMovementActionEndReason EndReason);

    void ApplyQueuedGrappleEnd();

    void StopMovementAction(EDRMovementActionEndReason EndReason);
    
    // 각 로컬 예측 및 서버 권한 인스턴스에서 이동이 시작된 이후의 경과 시간을 반환한다.
    float GetMovementElapsedTime() const;

#pragma region GameplayCue
    // 성공한 훅 위치와 표면 방향을 모든 클라이언트의 지속형 GameplayCue에 전달한다.
    void StartGrappleGameplayCue(const FVector& InHookLocation, const FVector& InHookNormal);
    
    // EndAbility의 모든 종료 경로에서 지속형 GameplayCue를 제거한다.
    void StopGrappleGameplayCue();
    
    void PlayFailedGrappleGameplayCue(const FVector& FailedLocation);

#pragma endregion
    
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
    TObjectPtr<UDRItemDefinition> ActiveItemDefinition;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelEventTask;

    FGuid ActiveInstanceId;
    FVector HookLocation = FVector::ZeroVector;
    // 훅이 부착된 순간의 월드 공간 표면 노멀이다.
    // 보호 시간이 끝난 뒤 캐릭터가 훅 평면 반대편으로 넘어갔는지 판정한다.
    FVector HookSurfaceNormal = FVector::ZeroVector;
    float MovementStartTimeSeconds = -1.f;

    FTimerHandle EndGrappleTimerHandle;

    EDRMovementActionEndReason PendingEndReason = EDRMovementActionEndReason::Invalidated;

    bool bMovementStarted = false;
    bool bMovementActionTagApplied = false;
    bool bEndQueued = false;
    bool bEndingGrapple = false;
    bool bGrappleGameplayCueActive = false;
};
