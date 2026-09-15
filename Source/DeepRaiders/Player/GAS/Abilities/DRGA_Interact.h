// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DeepRaiders/Core/Interaction/DRInteractionTypes.h"
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
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	bool SendLocalInteractionAttempt(EDRInteractionValidationResult& OutFailureResult);
	
	bool RegisterTargetDataDelegate();
	void UnregisterTargetDataDelegate();
	
	// SetTargetData가 변경되면 호출되는 함수
	// Target을 검증하고 Interact를 시도한다.
	void HandleServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);
	
	// TargetData에서 Target을 추출
	EDRInteractionValidationResult ExtractTargetActor(const FGameplayAbilityTargetDataHandle& TargetData,
		AActor*& OutTarget) const;
	
	// 서버에서 실제로 Interact를 시도
	EDRInteractionValidationResult ExecuteServerInteraction(AActor* Target) const;
	
	void LogInteractionFailure(EDRInteractionValidationResult Result, AActor* Target) const;
	
	FDelegateHandle TargetDataDelegateHandle;
};
