#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRPerkDefinition.generated.h"

class UGameplayEffect;

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

	/** 효과가 캐릭터 ASC에 적용되는지, 스킬이 설정을 읽어 변경되는지 결정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	EDRPerkEffectTarget EffectTarget = EDRPerkEffectTarget::OwnerCharacter;

	/** 스킬 동작 변경형 퍽을 식별하기 위한 태그다. 예: Perk.Skill.Charges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Skill")
	FGameplayTag PerkTag;

	/** GameplayEffect의 SetByCaller에 전달할 시트 기반 퍽 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Balance")
	TMap<FGameplayTag, float> EffectValues;

	/** 퍽 획득 시 직접 적용할 GameplayEffect다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|GAS")
	TSubclassOf<UGameplayEffect> PerkEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|GAS")
	uint8 bPersistThroughDeath:1 = true;
};
