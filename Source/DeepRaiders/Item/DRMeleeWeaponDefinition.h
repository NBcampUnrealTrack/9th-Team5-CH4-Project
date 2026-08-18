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
	/** 일반 적에게 적용할 기본 피해 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 40.f;

	/**
	 * 한 번의 공격이 유지되는 시간.
	 * 현재는 다음 공격 가능 시점도 이 값을 기준으로 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Attack", meta = (ClampMin = "0.01", Units = "s"))
	float AttackDuration = 0.8f;

	/**
	 * ViewLine 판정일 때
	 * 공격 시작 후 실제 판정을 수행할 시점.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float AttackHitTime = 0.25f;

	/** ViewLine 공격 사거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 200.f;

	/** WeaponSweep 판정 반경 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float SweepRadius = 35.f;

	/**
	 * 적에게 Damage를 전달할 GE.
	 * 현재는 BP_GE_Damage 사용.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee|GAS")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
