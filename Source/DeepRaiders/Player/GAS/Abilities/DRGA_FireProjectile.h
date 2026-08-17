// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "DRGA_FireProjectile.generated.h"

class UDRProjectileWeaponItemDefinition;

// 공용 Projectile 발사 GA
UCLASS()
class DEEPRAIDERS_API UDRGA_FireProjectile : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_FireProjectile();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle
		, const FGameplayAbilityActorInfo* ActorInfo
		, const FGameplayAbilityActivationInfo ActivationInfo
		, const FGameplayEventData* TriggerEventData) override;
	
private:
	// Definition 설정으로 적중 EffectSpec 생성
	void BuildImpactEffectSpecs(UAbilitySystemComponent* AbilitySystemComponent
		, UDRProjectileWeaponItemDefinition* WeaponDefinition
		, TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;
	
	bool SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo
		, UDRProjectileWeaponItemDefinition* WeaponDefinition
		, const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const;
};
