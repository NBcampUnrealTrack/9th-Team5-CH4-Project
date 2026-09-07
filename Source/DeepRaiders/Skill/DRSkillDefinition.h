#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DRSkillDefinition.generated.h"

class UDRGA_CharacterSkillBase;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSkillDataTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag CooldownTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CooldownDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDRSkillSlot SkillSlot = EDRSkillSlot::One;
};

/** 지정된 슬롯에 장착할 스킬 정의다. */
UCLASS(
	BlueprintType,
	AutoExpandCategories = ("Skill"),
	HideCategories = ("Item|GAS"))
class DEEPRAIDERS_API UDRSkillDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRSkillDefinition();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context) const override;
#endif

	/** 슬롯 배치와 독립적인 스킬 고유 식별자다. 예: Ability.Skill.Blink */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	FGameplayTag SkillId;

	/** 이 스킬만 차단하는 쿨다운 태그다. 예: Cooldown.Skill.Blink */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Cooldown")
	FGameplayTag CooldownTag;

	/** 이 스킬의 기본 쿨다운 시간이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Cooldown", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float CooldownDuration = 1.0f;

	/** 이 스킬이 장착되는 스킬 슬롯이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	EDRSkillSlot SkillSlot = EDRSkillSlot::One;

	/** 지정된 슬롯 입력에 지급할 스킬 Ability다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UDRGA_CharacterSkillBase> SkillAbility;

	/** 이 스킬이 기본으로 가지는 추가 GameplayEffect 규칙이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Effects")
	TArray<FDRSkillEffectRule> BaseEffectRules;
};
