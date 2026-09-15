#pragma once

#include "CoreMinimal.h"
#include "DRGA_ProjectileSkillBase.h"
#include "DRGA_GrabSkill.generated.h"

class ADRGrabProjectile;

UCLASS()
class DEEPRAIDERS_API UDRGA_GrabSkill : public UDRGA_ProjectileSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_GrabSkill();

protected:
	float GetPerkValue(FGameplayTag PerkTag, FGameplayTag ValueTag) const;
	TArray<FGameplayEffectSpecHandle> BuildImpactEffectSpecs() const;
	FGameplayEffectSpecHandle BuildArrivalSlowSpec() const;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab")
	TSubclassOf<ADRGrabProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab")
	TSubclassOf<UGameplayEffect> SnowCostEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PullSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "0.01", Units = "s"))
	float MaxPullDuration = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "0.0", Units = "cm"))
	float PullDestinationDistance = 150.f;
};
