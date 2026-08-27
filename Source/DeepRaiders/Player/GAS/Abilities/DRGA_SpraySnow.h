#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
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
	void BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;

	// ================================
	// Team
	// ================================

	bool IsFriendlyTarget(const AActor* TargetActor) const;
	int32 GetSourceTeamId() const;

	// ================================
	// Config
	// ================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Tick", meta = ( AllowPrivateAccess, ClampMin = "0.02", UIMin = "0.02", Units = "s"))
	float SprayTickInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Range", meta = ( AllowPrivateAccess, ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float SprayRange = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Range", meta = ( AllowPrivateAccess, ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0", Units = "deg"))
	float SprayHalfAngleDegrees = 20.f;

	/*
	 * Gameplay 판정용 Origin.
	 * 애니메이션 / Weapon Socket에 의존하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Origin", meta = ( AllowPrivateAccess, Units = "cm"))
	float SprayOriginForwardOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Origin", meta = ( AllowPrivateAccess, Units = "cm"))
	float SprayOriginHeightOffset = 60.f;

	const UDRSprayerWeaponDefinition* GetSprayerDefinition() const;
	/*
	 * 권장 순서:
	 * [0] FrozenDamage
	 * [1] FreezeGain
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Effect", meta = (AllowPrivateAccess))
	TArray<FDRGameplayEffectData> ImpactEffects;

	FTimerHandle SprayTimerHandle;

#if ENABLE_DRAW_DEBUG
public:
	void StartLocalDebugDraw();
	void StopLocalDebugDraw();
	void HandleDebugDrawTick();
	void DrawDebugSpray(
		const FVector& Origin,
		const FVector& Direction) const;


#endif
	
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Sprayer|Debug")
	bool bDrawDebugSpray = false;

	FTimerHandle DebugDrawTimerHandle;
};

