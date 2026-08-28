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
	void ExecuteConfirmedThrow();
	
	bool ValidateServerTargetData(FVector& OutLaunchLocation, FVector& OutLaunchDirection) const;
	
	bool SpawnServerProjectile(const FVector& LaunchLocation, const FVector& LaunchDirection);
	
	void BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;
	
	int32 GetSourceTeamId() const;
	
	void CancelThrow();
	
	UFUNCTION()
	void HandleAimReleased(float TimeHeld);
	
	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);
	
	UFUNCTION()
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData);
	
	UFUNCTION()
	void HandleBlockingStateAdded();
	
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	TSubclassOf<ADRThrowTargetActor> TargetActorClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	FDRThrowActionSettings ActionSettings;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;
	
	UPROPERTY(Transient)
	TObjectPtr<UDRThrowableItemDefinition> ActiveDefinition;
	
	FGameplayAbilityTargetDataHandle ConfirmedTargetData;
	FGuid ActiveInstanceId;	
};
