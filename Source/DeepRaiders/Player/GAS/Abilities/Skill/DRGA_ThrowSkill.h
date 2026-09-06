#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_ThrowSkill.generated.h"

class ADRThrowTargetActor;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UDRThrowableItemDefinition;

/** 기존 투척 아이템 정의를 페이로드로 사용하여 조준 후 투척하는 캐릭터 스킬 */
UCLASS()
class DEEPRAIDERS_API UDRGA_ThrowSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_ThrowSkill();

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

	virtual bool SpawnServerProjectile(
		const FVector& LaunchLocation,
		const FVector& LaunchDirection);

	const UDRThrowableItemDefinition* GetActiveDefinition() const
	{
		return ActiveDefinition;
	}

	const FDRThrowActionSettings& GetThrowActionSettings() const
	{
		return ActionSettings;
	}

	void ExecuteThrowGameplayCue(
		const FVector& LaunchLocation,
		const FVector& LaunchDirection);

	int32 GetSourceTeamId() const;

private:
	void StartTargeting();
	void StartBlockingStateTasks();
	void StartThrowMontage();
	void SetThrowAimState(bool bEnable);
	void ExecuteConfirmedThrow();
	void CancelThrow();

	bool ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutAimDirection) const;
	bool ResolveServerLaunchData(FVector& OutLaunchLocation, FVector& OutLaunchDirection) const;

	void BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;

	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleBlockingStateAdded();

	UFUNCTION()
	void HandleThrowReleaseEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Throw")
	TSubclassOf<ADRThrowTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Throw", meta = (AllowPrivateAccess = "true"))
	FDRThrowActionSettings ActionSettings;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ReleaseEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UDRThrowableItemDefinition> ActiveDefinition;

	FVector ValidatedAimDirection = FVector::ZeroVector;

	bool bReleaseEventReceived = false;
	bool bUsingThrowAimState = false;
	bool bEndingThrow = false;
};
