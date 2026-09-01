#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_SlowProjectileSkill.generated.h"

class ADRSlowProjectile;
class UGameplayEffect;

UCLASS()
class DEEPRAIDERS_API UDRGA_SlowProjectileSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile")
	TSubclassOf<ADRSlowProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile", meta = (ClampMin = "1.0", Units = "cm"))
	float EffectRadius = 300.f;
};
