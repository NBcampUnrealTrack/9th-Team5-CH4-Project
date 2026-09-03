#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DRGA_ProjectileSkillBase.h"
#include "DRGA_SpearThrowSkill.generated.h"

class ADRSpearProjectile;
class UAbilitySystemComponent;

UCLASS()
class DEEPRAIDERS_API UDRGA_SpearThrowSkill : public UDRGA_ProjectileSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw")
	TSubclassOf<ADRSpearProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float KnockbackStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxAimDistance = 15000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw|Effect")
	TArray<FDRGameplayEffectData> ImpactEffects;

private:
	bool SpawnProjectile(
		ADRPlayerCharacter* Character,
		UAbilitySystemComponent* AbilitySystem,
		const FVector& SpawnLocation,
		const FVector& ProjectileDirection) const;

	void BuildImpactEffectSpecs(
		UAbilitySystemComponent* AbilitySystem,
		TArray<FGameplayEffectSpecHandle>& OutImpactEffectSpecs) const;
};
