#pragma once

#include "CoreMinimal.h"
#include "DRThrowableProjectile.h"
#include "DRSlowProjectile.generated.h"

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRSlowProjectile : public ADRThrowableProjectile
{
	GENERATED_BODY()

public:
	void InitializeSlowProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		const FGameplayEffectSpecHandle& InSlowEffectSpec,
		const FGameplayEffectSpecHandle& InAllySpeedEffectSpec,
		const FDRThrowableItemSettings& InItemSettings,
		const FDRThrowActionSettings& InActionSettings,
		int32 InSourceTeamId,
		const UObject* InPresentationSourceObject);

protected:
	virtual bool ShouldAffectInstigator() const override
	{
		return AllySpeedEffectSpec.IsValid();
	}

	virtual bool IsValidEffectTarget(const AActor* TargetActor) const override;
	virtual void ApplyEffectToTarget(AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystem, const FHitResult& ImpactResult) override;
	virtual void HandleWorldImpact(const FHitResult& ImpactResult) override
	{
	}

private:
	bool ApplySpeedEffectToTarget(
		UAbilitySystemComponent* TargetAbilitySystem,
		const FGameplayEffectSpecHandle& EffectSpec,
		const FHitResult& ImpactResult);

	FGameplayEffectSpecHandle SlowEffectSpec;
	FGameplayEffectSpecHandle AllySpeedEffectSpec;
};
