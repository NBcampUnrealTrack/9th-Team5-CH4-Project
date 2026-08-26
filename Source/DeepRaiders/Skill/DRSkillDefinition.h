#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DRSkillDefinition.generated.h"

class UDRGA_CharacterSkillBase;

/** 상점에서 구매해 지정된 슬롯에 장착할 스킬 정의다. */
UCLASS(
	BlueprintType,
	AutoExpandCategories = ("Skill"),
	HideCategories = ("Item|GAS"))
class DEEPRAIDERS_API UDRSkillDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRSkillDefinition();

	/** 이 스킬이 장착되는 스킬 슬롯이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	EDRSkillSlot SkillSlot = EDRSkillSlot::One;

	/** 구매 시 지정된 슬롯 입력에 지급할 스킬 Ability다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UDRGA_CharacterSkillBase> SkillAbility;
};
