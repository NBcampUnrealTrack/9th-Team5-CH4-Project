#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_TestToggleFrozen.generated.h"

class UGameplayEffect;

UCLASS()
class DEEPRAIDERS_API UDRGA_TestToggleFrozen
	: public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_TestToggleFrozen();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Test")
	TSubclassOf<UGameplayEffect> FrozenEffectClass;
};