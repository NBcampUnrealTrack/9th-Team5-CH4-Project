#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DRSkillTypes.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EDRSkillSlot : uint8
{
	One,
	Two,
	Count UMETA(Hidden)
};

/** 스킬 발동 흐름에서 GameplayEffect를 적용할 시점이다. */
UENUM(BlueprintType)
enum class EDRSkillEffectTrigger : uint8
{
	/** Commit이 성공한 직후 한 번 적용한다. Instant GE에 사용한다. */
	OnSkillCommitted,

	/** 스킬이 활성화된 동안 적용하고, 종료 또는 취소 시 회수한다. */
	WhileSkillActive,

	/** 스킬이 취소되지 않고 정상적으로 종료된 직후 한 번 적용한다. */
	OnSkillCompleted,

	/** 스킬별 실제 착지 판정이 성공한 직후 한 번 적용한다. 현재 슈퍼점프가 이 이벤트를 발생시킨다. */
	OnSkillLanded
};

/** 퍽의 개별 EffectRule이 퍽 정의의 기존 EffectTarget을 선택적으로 재정의하는 방식이다. */
UENUM(BlueprintType)
enum class EDRSkillEffectTargetOverride : uint8
{
	/**
	 * TargetOverride 도입 전에 만들어진 기존 퍽 에셋의 처리 결과를 그대로 유지하기 위한 호환 값이다.
	 * 이 값이면 Rule이 대상을 재정의하지 않고 UDRPerkDefinition::EffectTarget을 그대로 사용한다.
	 * 기존 에셋은 별도 수정이나 재저장 없이 모두 이 값으로 동작한다.
	 */
	UsePerkDefault UMETA(DisplayName = "Use Perk Default (Legacy Compatibility)"),

	/** 이 Rule의 GameplayEffect를 소유 캐릭터 ASC에 적용한다. */
	OwnerCharacter,

	/** 이 Rule의 완성된 GameplayEffectSpec을 장착된 스킬의 실행 대상에 전달한다. */
	EquippedSkill
};

/**
 * 스킬 기본 효과와 장착 퍽 효과가 공통으로 사용하는 규칙이다.
 * 실행부는 EffectClass와 EffectValues를 완성된 GameplayEffectSpec으로 변환하며,
 * 대상에 따라 소유 캐릭터에 즉시 적용하거나 장착된 스킬의 실행 대상으로 전달한다.
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSkillEffectRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	EDRSkillEffectTrigger Trigger = EDRSkillEffectTrigger::OnSkillCommitted;

	/**
	 * 퍽의 기존 EffectTarget을 Rule 단위로 재정의한다.
	 * UsePerkDefault는 TargetOverride 도입 전 에셋의 처리 방식을 유지하는 호환용 기본값이다.
	 * 하나의 신규 퍽에서 OwnerCharacter와 EquippedSkill Rule을 함께 사용할 때만 다른 값을 선택한다.
	 * UDRSkillDefinition::BaseEffectRules에서는 이 값이 사용되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	EDRSkillEffectTargetOverride TargetOverride =
		EDRSkillEffectTargetOverride::UsePerkDefault;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** 생성할 GameplayEffectSpec에 복사되는 SetByCaller 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	TMap<FGameplayTag, float> EffectValues;
};
