#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/Skill/DRGA_ThrowSkill.h"
#include "DRGA_SlowProjectileSkill.generated.h"

class ADRSlowProjectile;
class UGameplayEffect; 
class UDRPerkComponent;
class UDRSkillDefinition;

UCLASS()
class DEEPRAIDERS_API UDRGA_SlowProjectileSkill : public UDRGA_ThrowSkill
{
	GENERATED_BODY()

protected:
	virtual bool SpawnServerProjectile(
		const FVector& LaunchLocation,
		const FVector& LaunchDirection) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile")
	TSubclassOf<ADRSlowProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile")
	float BaseSlowMagnitude = -0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile|Perks")
	TSubclassOf<UGameplayEffect> ExtremeSlowEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile|Perks")
	TSubclassOf<UGameplayEffect> AllySpeedEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Slow Projectile", meta = (ClampMin = "1.0", Units = "cm"))
	float EffectRadius = 300.f;

private:
	bool BuildEffectSpecs(
		const UDRSkillDefinition* SkillDefinition,
		const UDRPerkComponent* PerkComponent,
		FGameplayEffectSpecHandle& OutSlowSpec,
		FGameplayEffectSpecHandle& OutAllySpec) const;
};
