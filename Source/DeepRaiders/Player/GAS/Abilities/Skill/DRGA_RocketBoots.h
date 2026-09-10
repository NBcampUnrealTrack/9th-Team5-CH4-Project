#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_RocketBoots.generated.h"

class ADRPlayerCharacter;

/** SuperJump 체공 중 점프 입력으로 한 번 사용하는 로켓 전투화 수평 돌진이다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_RocketBoots : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_RocketBoots();

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
		bool IsReplicateEndAbility,
		bool IsWasCancelled) override;

	/** 점프 입력 순간의 수평 방향으로 더하는 속도 변화량. 체공 중 목표 위치를 고정하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Rocket Boots", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float ImpulseStrength = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Rocket Boots", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DashGameplayCueDuration = 0.2f;

private:
	void StartDashGameplayCue(ADRPlayerCharacter* Character, const FVector& DashDirection);
	void StopDashGameplayCue();

	UFUNCTION()
	void HandleDashGameplayCueFinished();

	/** 마지막으로 돌진을 소비한 SuperJump 실행 번호다. */
	uint32 ConsumedSuperJumpSequence = 0;
	bool IsDashGameplayCueActive = false;
};
