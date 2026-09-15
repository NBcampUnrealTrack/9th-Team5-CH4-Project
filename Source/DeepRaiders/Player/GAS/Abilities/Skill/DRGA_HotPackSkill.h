#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_HotPackSkill.generated.h"

class ADRHotPackArea;
class UGameplayEffect;

UCLASS()
class DEEPRAIDERS_API UDRGA_HotPackSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Hot Pack")
	TSubclassOf<ADRHotPackArea> HotPackAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Hot Pack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AreaRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Hot Pack", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float AreaDuration = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Hot Pack")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Hot Pack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealthRecoveryAmount = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Hot Pack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FreezeGaugeRecoveryAmount = 7.5f;
};
