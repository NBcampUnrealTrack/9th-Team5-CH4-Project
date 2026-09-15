#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/Skill/DRGA_ThrowSkill.h"
#include "DRGA_InstantCareSkill.generated.h"

class ADRInstantCareProjectile;
class UGameplayEffect;
class UDRThrowableItemDefinition;

UCLASS()
class DEEPRAIDERS_API UDRGA_InstantCareSkill : public UDRGA_ThrowSkill
{
	GENERATED_BODY()

public:
	UDRGA_InstantCareSkill();

protected:
	virtual bool SpawnServerProjectile(
		const FVector& LaunchLocation,
		const FVector& LaunchDirection) override;
	virtual UDRThrowableItemDefinition* ResolveThrowableDefinition(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care")
	TObjectPtr<UDRThrowableItemDefinition> ThrowableDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care")
	TSubclassOf<ADRInstantCareProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float RecoveryRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Instant Care", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealthRecoveryAmount = 2.5f;
};
