// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_ToggleInventory.generated.h"

/**
 * 
 */
UCLASS()
class DEEPRAIDERS_API UDRGA_ToggleInventory : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_ToggleInventory();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
