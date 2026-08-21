// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/Snow/Components/DRSnowRemoveComponent.h"
#include "DRGA_AbsorbSnow.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow|Absorb")
	TSubclassOf<UGameplayEffect> SnowGainEffectClass;

private:
	UFUNCTION()
	void HandleAbsorbDelayFinished();

	void PerformAbsorbTick();
	void ScheduleNextAbsorbTick();

	bool BuildRemovalSpec(UAbilitySystemComponent* AbilitySystemComponent, FDRSnowRemovalSpec& OutRemovalSpec) const;
	bool TraceSnowTarget(const FGameplayAbilityActorInfo* ActorInfo, FHitResult& OutHitResult) const;
	void ApplySnowGaugeGain(UAbilitySystemComponent* AbilitySystemComponent, float RemovedAmount) const;
};
