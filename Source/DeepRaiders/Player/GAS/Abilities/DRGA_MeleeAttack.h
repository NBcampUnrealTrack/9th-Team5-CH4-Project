#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_MeleeAttack.generated.h"

class UDRMeleeCombatComponent;
class UDRMeleeWeaponItemDefinition;

UCLASS()
class DEEPRAIDERS_API UDRGA_MeleeAttack
	: public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_MeleeAttack();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

private:
	void HandleMeleeHit(const FHitResult& HitResult);

	void ExecuteSoundCue(
		const FGameplayTag& SoundCueTag,
		AActor* SourceActor,
		const FVector& Location) const;

	TWeakObjectPtr<UDRMeleeCombatComponent> ActiveMeleeComponent;
	TWeakObjectPtr<UDRMeleeWeaponItemDefinition> ActiveWeaponDefinition;

	FDelegateHandle MeleeHitDelegateHandle;
};