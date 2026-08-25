#pragma once

#include "CoreMinimal.h"
#include "DRGA_CharacterSkillBase.h"
#include "DRGA_TestSkill.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGA_TestSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
