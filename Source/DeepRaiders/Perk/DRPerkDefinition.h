#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DRPerkDefinition.generated.h"

class UGameplayEffect;
class UDRSkillDefinition;

UENUM(BlueprintType)
enum class EDRPerkTrigger : uint8
{
	WhileEquipped,
	OnSkillCommitted
};

UENUM(BlueprintType)
enum class EDRPerkEffectTarget : uint8
{
	OwnerCharacter,
	EquippedSkill
};

UCLASS(BlueprintType, AutoExpandCategories = ("Perk"))
class DEEPRAIDERS_API UDRPerkDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRPerkDefinition();

	/** 비어 있으면 기존 공용 퍽처럼 어느 스킬에도 연결하지 않고 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	FGameplayTagContainer CompatibleSkillTags;

	/** 장착 중 상시 효과인지, 장착한 스킬의 사용 성공 시 효과인지 결정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	EDRPerkTrigger Trigger = EDRPerkTrigger::WhileEquipped;

	/**
	 * 이 퍽 EffectRule의 기본 적용 대상이다.
	 * Rule의 TargetOverride가 UsePerkDefault인 기존 에셋은 이 값을 그대로 사용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	EDRPerkEffectTarget EffectTarget = EDRPerkEffectTarget::OwnerCharacter;

	/** 스킬 동작 변경형 퍽을 식별하기 위한 태그다. 예: Perk.Skill.Charges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	FGameplayTag PerkTag;

	/** 구매 시 호환 스킬을 대체할 스킬이다. 판매 또는 초기화 시 기존 스킬로 복구한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	TObjectPtr<UDRSkillDefinition> ReplacementSkillDefinition;

	/** 퍽 획득 시 직접 적용할 GameplayEffect다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|GAS")
	TSubclassOf<UGameplayEffect> PerkEffectClass;

	/**
	 * 이 퍽이 장착된 스킬의 발동 흐름에 추가할 효과들이다.
	 * 빈 배열이면 기존 PerkEffectClass / Trigger 설정을 호환용으로 사용한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill Effects")
	TArray<FDRSkillEffectRule> EffectRules;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|GAS")
	uint8 bPersistThroughDeath:1 = true;
};
