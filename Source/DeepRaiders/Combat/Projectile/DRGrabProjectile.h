#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRGrabProjectile.generated.h"

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRGrabProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRGrabProjectile(const FObjectInitializer& ObjectInitializer);

	void InitializeGrabProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		int32 InSourceTeamId,
		float InMaxDistance,
		float InPullSpeed,
		float InPullDestinationDistance);

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void HandleImpact(const FHitResult& ImpactResult) override;

private:
	void StartGrabGameplayCue();

	void StopGrabGameplayCue();

	void PullTarget(const FHitResult& ImpactResult) const;

	FVector LaunchLocation = FVector::ZeroVector;
	float MaxDistance = 0.f;
	float PullSpeed = 0.f;
	float PullDestinationDistance = 0.f;
	bool IsGrabGameplayCueActive = false;
};
