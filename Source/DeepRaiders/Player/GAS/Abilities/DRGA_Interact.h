// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_Interact.generated.h"

class AActor;
class APawn;
class UWorld;

/*
 * 범용 상호작용 GA
 */
UCLASS()
class DEEPRAIDERS_API UDRGA_Interact : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_Interact();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	AActor* FindBestInteractionTarget(APawn* Interactor) const;
	
	// 상호작용 대상이 다른 물체에 가려지지 않는지 체크
	bool HasClearLineOfSight(UWorld* World, APawn* Interactor, AActor* Target, const FVector& ViewLocation, const FVector& TargetLocation) const;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxInteractionDistance = 300.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.1", ClampMax = "89.0", Units = "deg"))
	float MaxInteractionAngleDegrees = 6.f;
	
};
