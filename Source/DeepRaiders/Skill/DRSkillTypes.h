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
	OnSkillCompleted
};

/** 스킬 기본 효과와 장착 퍽 효과가 공통으로 사용하는 GE 적용 규칙이다. */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSkillEffectRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	EDRSkillEffectTrigger Trigger = EDRSkillEffectTrigger::OnSkillCommitted;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** 적용할 GE의 SetByCaller 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effect")
	TMap<FGameplayTag, float> EffectValues;
};
