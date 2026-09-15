#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "ActiveGameplayEffectHandle.h"
#include "DRGA_SpraySnow.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UDRSprayerWeaponDefinition;

UCLASS()
class DEEPRAIDERS_API UDRGA_SpraySnow : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_SpraySnow();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// ================================
	// Ability Lifecycle
	// ================================

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	void StartServerSpray();
	void StopServerSpray();

	void HandleSprayTick();

	// ================================
	// Spray
	// ================================

	bool ResolveSprayOriginAndDirection(FVector& OutOrigin, FVector& OutDirection) const;
	void ApplySprayToTargets(const FVector& Origin, const FVector& Direction);
	bool HasLineOfSightToTarget(const FVector& Origin, const AActor* TargetActor, const FCollisionQueryParams& QueryParams) const;

	// ================================
	// GAS
	// ================================

	bool TryConsumeSnowCost();
	void ApplyHeatForSuccessfulSprayTick();
	void BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;
	float GetWeaponStatMultiplier(const FGameplayAttribute& Attribute) const;

	// ================================
	// Team
	// ================================

	bool IsFriendlyTarget(const AActor* TargetActor) const;
	int32 GetSourceTeamId() const;

	const UDRSprayerWeaponDefinition* GetSprayerDefinition() const;

	FTimerHandle SprayTimerHandle;
	
	// Monatage
	void StartSprayMontage();
	void StopSprayMontage();
	
	TMap<TWeakObjectPtr<AActor>, float> LastHitReactionTimes;

	void TryExecuteHitReaction(AActor* TargetActor, UAbilitySystemComponent* TargetAbilitySystem, const FVector& SprayOrigin);
	
	FActiveGameplayEffectHandle ActivePresentationEffectHandle;

	void StartSprayPresentation();
	void StopSprayPresentation();
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Sprayer|Debug")
	bool bDrawDebugSpray = false;

	FTimerHandle DebugDrawTimerHandle;
	
#if ENABLE_DRAW_DEBUG
	void StartLocalDebugDraw();
	void StopLocalDebugDraw();
	void HandleDebugDrawTick();
	void DrawDebugSpray(
		const FVector& Origin,
		const FVector& Direction) const;
#endif
	
};

