#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRGrabProjectile.generated.h"

class ADRPlayerCharacter;
enum class EDRMovementActionEndReason : uint8;

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
		float InPullDestinationDistance,
		const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs = {},
		float InHitScale = 1.f,
		const FGameplayEffectSpecHandle& InArrivalSlowSpec = {},
		float InMaxPullDuration = 15.f);

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_Instigator() override;
	virtual void OnRep_AttachmentReplication() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void HandleImpact(const FHitResult& ImpactResult) override;

private:
	void StartGrabGameplayCue();

	void StopGrabGameplayCue();
	void HandleGrabGameplayCue(EGameplayCueEvent::Type EventType);
	void StopProjectileMotion();
	void ReleasePullTarget();

	void PullTarget(const FHitResult& ImpactResult);
	void UpdatePullState();
	void HandlePullEnded(EDRMovementActionEndReason Reason);
	void ApplyArrivalSlow(ADRPlayerCharacter* TargetCharacter) const;

	FGameplayEffectSpecHandle ArrivalSlowSpec;
	TWeakObjectPtr<ADRPlayerCharacter> PulledCharacter;
	FDelegateHandle PullEndedHandle;
	int32 PullSessionId = 0;
	bool IsPulling = false;

	FVector LaunchLocation = FVector::ZeroVector;
	float MaxDistance = 0.f;
	float PullSpeed = 0.f;
	float MaxPullDuration = 15.f;
	float PullDestinationDistance = 0.f;
	bool IsGrabGameplayCueActive = false;
};
