#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_GrabSkill.generated.h"

class ADRGrabProjectile;

UCLASS()
class DEEPRAIDERS_API UDRGA_GrabSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_GrabSkill();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab")
	TSubclassOf<ADRGrabProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PullSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Grab", meta = (ClampMin = "0.0", Units = "cm"))
	float PullDestinationDistance = 150.f;
};
