#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DeepRaiders/Item/DRThrowableItemTypes.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_InstantCareSkill.generated.h"

class ADRInstantCareProjectile;
class UGameplayEffect;

UCLASS()
class DEEPRAIDERS_API UDRGA_InstantCareSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care")
	TSubclassOf<ADRInstantCareProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care", meta = (ShowOnlyInnerProperties))
	FDRThrowActionSettings ActionSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float RecoveryRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealthRecoveryAmount = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care|Throw", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm/s"))
	float InitialSpeed = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care|Throw", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GravityScale = 1.0f;
};
