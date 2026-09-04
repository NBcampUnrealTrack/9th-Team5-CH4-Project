#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DRGA_ThrowItem.generated.h"

class ADRThrowTargetActor;
class UAbilityTask_WaitTargetData;
class UDRInventoryComponent;
class UDRThrowableItemDefinition;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;

UCLASS()
class DEEPRAIDERS_API UDRGA_ThrowItem : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_ThrowItem();
	
protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
private:
	bool ResolveSelectedThrowable(const FGameplayAbilityActorInfo* ActorInfo,
		const UDRThrowableItemDefinition* ExpectedDefinition,
		UDRInventoryComponent*& OutInventory, FGuid& OutInstanceId) const;
	
	int32 ResolveInputId(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	
	void StartTargeting(int32 InputId);
	void StartBlockingStateTasks();
	void SetThrowAimState(bool bEnable);
	void ExecuteConfirmedThrow();
	
	bool ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutAimDirection) const;
	bool ResolveServerLaunchData(FVector& OutLaunchLocation, FVector& OutLaunchDirection) const;
	
	bool SpawnServerProjectile(const FVector& LaunchLocation, const FVector& LaunchDirection);
	
	void ExecuteThrowGameplayCue(const FVector& LaunchLocation, const FVector& LaunchDirection);
	
	void BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;
	
	int32 GetSourceTeamId() const;
	
	void CancelThrow();
	
	UFUNCTION()
	void HandleAimInputReleased(float TimeHeld);
	
	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);
	
	UFUNCTION()
	void HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData);
	
	UFUNCTION()
	void HandleBlockingStateAdded();
	
	void StartThrowMontage();
	
	UFUNCTION()
	void HandleThrowReleaseEvent(FGameplayEventData Payload);
	
	UFUNCTION()
	void HandleMontageCompleted();
	
	UFUNCTION()
	void HandleMontageInterrupted();
	
private:
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	TSubclassOf<ADRThrowTargetActor> TargetActorClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	FDRThrowActionSettings ActionSettings;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;
	
	UPROPERTY(Transient)
	TObjectPtr<UDRThrowableItemDefinition> ActiveDefinition;
	
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ReleaseEventTask;
	
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> AimReleaseTask;
	
	bool bReleaseEventReceived = false;
	bool bUsingThrowAimState = false;
	
	FGuid ActiveInstanceId;	
	
	FVector ValidatedAimDirection = FVector::ZeroVector;	
};
