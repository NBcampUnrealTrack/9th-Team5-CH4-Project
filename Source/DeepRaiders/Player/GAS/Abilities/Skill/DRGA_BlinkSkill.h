#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_BlinkSkill.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGA_BlinkSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Blink", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float BlinkDistance = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Blink", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float RecoveryDuration = 0.5f;
};
