#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRSpearProjectile.generated.h"

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRSpearProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	void InitializeSpearProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
		int32 InSourceTeamId,
		float InKnockbackStrength);

protected:
	virtual void HandleImpact(const FHitResult& ImpactResult) override;

private:
	void ApplyKnockback(const FHitResult& ImpactResult) const;

	float KnockbackStrength = 0.f;
};
