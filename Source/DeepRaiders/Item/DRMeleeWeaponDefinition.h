#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRMeleeWeaponDefinition.generated.h"

class UGameplayEffect;

/**
 * 근접 무기 전용 Definition.
 *
 * 공통 Item 데이터는 UDRItemDefinition이 담당하고,
 * 근접 전투에 필요한 수치만 이 클래스가 가진다.
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRMeleeWeaponItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRMeleeWeaponItemDefinition();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float SweepRadius = 35.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|GAS")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
