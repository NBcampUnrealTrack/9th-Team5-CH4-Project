#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DRGA_RangedWeaponAttack.h"
#include "DRGA_FireHitScan.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGA_FireHitScan : public UDRGA_RangedWeaponAttack
{
	GENERATED_BODY()
	
protected:
	virtual void OnRangedWeaponActivated() override;
	virtual void OnRangedWeaponEnded() override;
	virtual bool SendLocalShotRequest() override;
	
private:
	void RegisterTargetDataDelegate();
	void UnregisterTargetDataDelegate();
	
	void HandleServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);
	bool ValidateTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutServerViewLocation, FVector& OutAimDirection) const;
	
	FVector TraceHitScan(const FVector& TraceStart, const FVector& TraceEnd, bool bApplyServerEffects,
		const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const;
	
	FDelegateHandle TargetDataDelegateHandle;	
};
