#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_TestAddSnow.generated.h"

class UGameplayEffect;

UCLASS()
class DEEPRAIDERS_API UDRGA_TestAddSnow : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_TestAddSnow();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "GAS|Test")
	TSubclassOf<UGameplayEffect> AddSnowEffectClass;
};