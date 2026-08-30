#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "DeepRaiders/Combat/Grapple/DRGrappleAbilityTypes.h"
#include "DRGrappleTargetActor.generated.h"

class UNiagaraComponent;

UCLASS(Blueprintable, NotPlaceable)
class DEEPRAIDERS_API ADRGrappleTargetActor : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	ADRGrappleTargetActor();

	void Configure(const FDRGrappleAbilitySettings& InSettings);

	virtual void Tick(float DeltaSeconds) override;

	virtual void StartTargeting(UGameplayAbility* Ability) override;

	virtual bool IsConfirmTargetingAllowed() override;

	virtual void ConfirmTargetingAndContinue() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static bool IsValidGrappleSurface(const FHitResult& Hit);

private:
	bool UpdateTargeting();

	void UpdateAimMarker(const FHitResult& Hit);

	void DestroyAimMarker();

	FDRGrappleAbilitySettings Settings;
	FHitResult CachedAimHit;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AimMarkerComponent;

	bool bHasValidAimData = false;
};
