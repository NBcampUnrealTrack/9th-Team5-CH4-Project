#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_RocketBoots.generated.h"

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

private:
	bool ResolvePerkValues(
		const FGameplayAbilityActorInfo* ActorInfo,
		float& OutMoveSpeed) const;

	/** 마지막으로 돌진을 소비한 SuperJump 실행 번호다. */
	uint32 ConsumedSuperJumpSequence = 0;
};
