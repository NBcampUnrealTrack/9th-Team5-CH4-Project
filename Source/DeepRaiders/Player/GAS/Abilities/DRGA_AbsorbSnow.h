// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/Snow/Components/DRSnowRemoveComponent.h"
#include "DRGA_AbsorbSnow.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UAnimMontage;
class UDRRangedWeaponDefinition;

// Player가 바라보는 표면의 눈을 흡수해 SnowGauge로 전환하는 GA
UCLASS()
class DEEPRAIDERS_API UDRGA_AbsorbSnow : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_AbsorbSnow();
	
protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	TSubclassOf<UGameplayEffect> SnowGainEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb|Animation")
	TObjectPtr<UAnimMontage> AbsorbMontage = nullptr;
	
private:
	UFUNCTION()
	void HandleAbsorbDelayFinished();

	void PerformAbsorbTick();
	void ScheduleNextAbsorbTick();
	void StartAbsorbGameplayCue();
	void StopAbsorbGameplayCue();
	void HandleGaugeGainPulse(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UDRRangedWeaponDefinition* WeaponDefinition,
		float ActualGaugeGain);
	void ResetGaugeGainPulse();
	float RollGaugeGainPulseThreshold(const UDRRangedWeaponDefinition* WeaponDefinition) const;
	void ExecuteGaugeGainPulse(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UDRRangedWeaponDefinition* WeaponDefinition,
		float ReachedThreshold) const;

	bool BuildRemovalSpec(FDRSnowRemovalSpec& OutRemovalSpec) const;
	void ResetAbsorbSummary();
	void LogAbsorbSummary() const;
	float ApplySnowGaugeGain(
		UAbilitySystemComponent* AbilitySystemComponent,
		float RemovedAmount,
		float SolidRemovalPerSnowGauge) const;

	bool bAbsorbGameplayCueActive = false;
	float GaugeGainSinceLastPulse = 0.f;
	float NextGaugeGainPulseThreshold = 0.f;

	int32 AbsorbSummaryTickCount = 0;
	float AbsorbSummaryStartTime = 0.f;
	float AbsorbSummaryRemovedAmount = 0.f;
	float AbsorbSummaryGaugeGain = 0.f;
	float AbsorbSummaryPower = 0.f;
	float AbsorbSummarySolidRemovalPerGauge = 0.f;
};
