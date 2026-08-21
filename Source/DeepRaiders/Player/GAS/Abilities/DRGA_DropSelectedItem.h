// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_DropSelectedItem.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGA_DropSelectedItem : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_DropSelectedItem();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0.0", Units = "cm"))
	float DropForwardDistance = 100.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0.0", Units = "cm"))
	float DropHeightOffset = 50.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0.0"))
	float DropForwardImpulse = 250.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0.0"))
	float DropUpwardImpulse = 100.f;
};
