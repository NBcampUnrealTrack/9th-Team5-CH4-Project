#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_BarrierSkill.generated.h"

class ADRBarrierGenerator;

/** 사용 즉시 시전자 발밑에 배리어 생성기를 설치하는 범위 방어 스킬이다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_BarrierSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_BarrierSkill();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual bool ResolveBarrierSpawnTransform(
		ADRPlayerCharacter* Character,
		FTransform& OutSpawnTransform) const;

	/** GA Blueprint에서 생성기 모양/VFX를 가진 Actor 클래스를 교체할 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Barrier")
	TSubclassOf<ADRBarrierGenerator> BarrierGeneratorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Barrier", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float BarrierRadius = 400.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Barrier", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float BarrierDuration = 8.f;
	/** 배리어가 완전히 흡수할 수 있는 총 피해량이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Barrier", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BarrierMaxHealth = 300.f;
};
