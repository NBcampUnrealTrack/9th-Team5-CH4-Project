#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_SpearThrowSkill.generated.h"

class ADRSpearProjectile;
class UAbilitySystemComponent;

UCLASS()
class DEEPRAIDERS_API UDRGA_SpearThrowSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	void BuildImpactEffectSpecs(
		UAbilitySystemComponent* AbilitySystem,
		TArray<FGameplayEffectSpecHandle>& OutImpactEffectSpecs) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw")
	TSubclassOf<ADRSpearProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float KnockbackStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw|Effect")
	TArray<FDRGameplayEffectData> ImpactEffects;
};
