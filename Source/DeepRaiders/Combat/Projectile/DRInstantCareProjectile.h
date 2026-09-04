#pragma once

#include "CoreMinimal.h"
#include "DRThrowableProjectile.h"
#include "DRInstantCareProjectile.generated.h"

class UDrawSphereComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRInstantCareProjectile : public ADRThrowableProjectile
{
	GENERATED_BODY()

public:
	ADRInstantCareProjectile(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void HandleImpact(const FHitResult& ImpactResult) override;
	virtual bool ShouldAffectInstigator() const override { return true; }
	virtual bool IsValidEffectTarget(const AActor* TargetActor) const override;
	virtual void HandleTargetRejected(
		const AActor* TargetActor,
		EDRThrowableTargetRejectReason RejectReason) const override;
	virtual void ApplyEffectToTarget(
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FHitResult& ImpactResult) override;
	virtual void HandleWorldImpact(const FHitResult& ImpactResult) override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Instant Care")
	TObjectPtr<UDrawSphereComponent> ExplosionRadiusGizmo;
#endif
};
