#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRCannonProjectile.generated.h"

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRCannonProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRCannonProjectile(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void HandleImpact(const FHitResult& ImpactResult) override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cannon|Explosion", meta = ( AllowPrivateAccess, ClampMin = "1.0", Units = "cm"))
	float ExplosionRadius = 350.f;
};
