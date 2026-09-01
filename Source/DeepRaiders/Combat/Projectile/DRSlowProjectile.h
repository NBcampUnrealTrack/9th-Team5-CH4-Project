#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRSlowProjectile.generated.h"

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRSlowProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	void InitializeSlowProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		const FGameplayEffectSpecHandle& InSlowEffectSpec,
		int32 InSourceTeamId,
		float InEffectRadius);

protected:
	virtual void HandleImpact(const FHitResult& ImpactResult) override;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDisplayEffectRadius(
		const FVector& EffectLocation,
		float InEffectRadius);

	float EffectRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Slow", meta = (AllowPrivateAccess = true, ClampMin = "0.0", Units = "s"))
	float EffectRadiusDisplayDuration = 5.f;
};
