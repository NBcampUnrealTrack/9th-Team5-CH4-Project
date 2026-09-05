#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRCannonProjectile.generated.h"

class UDRSnowAddComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRCannonProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRCannonProjectile(const FObjectInitializer& ObjectInitializer);

	virtual float GetConfiguredInitialSpeed() const override;
	virtual float GetConfiguredGravityScale() const override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleImpact(const FHitResult& ImpactResult) override;
	
	virtual bool ShouldIgnoreFriendlyBlockingHit() const override
	{
		return false;
	}
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Snow", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDRSnowAddComponent> SnowAddComponent;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cannon|Explosion", meta = ( AllowPrivateAccess, ClampMin = "1.0", Units = "cm"))
	float ExplosionRadius = 350.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cannon|Debug", meta = (AllowPrivateAccess))
	bool bDrawExplosionDebug = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cannon|Explosion", meta = (AllowPrivateAccess))
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_Visibility;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cannon|Movement", meta = ( AllowPrivateAccess, ClampMin = "1.0", Units = "cm/s"))
	float InitialSpeed = 2200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cannon|Movement", meta = ( AllowPrivateAccess, ClampMin = "0.0"))
	float GravityScale = 1.15f;
};
