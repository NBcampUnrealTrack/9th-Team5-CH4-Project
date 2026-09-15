#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_ForwardDashSkill.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGA_ForwardDashSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool IsReplicateEndAbility,
		bool IsWasCancelled) override;

	UFUNCTION()
	void HandleDashFinished();

	/** 대쉬의 목표 수평 거리. 공중/지상 상태와 무관하게 충돌 전까지 이 거리만 이동한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Forward Dash", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DashDistance = 500.0f;

	/** 대쉬가 목표 거리에 도달하는 데 걸리는 시간. 내부 속도는 Distance / Duration으로 계산된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Forward Dash", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float DashDuration = 0.2f;

private:
	void StartDashGameplayCue(ADRPlayerCharacter* Character, const FVector& DashDirection);
	void StopDashGameplayCue();

	bool IsDashGameplayCueActive = false;
};
