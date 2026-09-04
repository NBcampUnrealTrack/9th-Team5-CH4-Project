#pragma once

#include "CoreMinimal.h"
#include "DRSkillDefinition.h"
#include "DRThrowSkillDefinition.generated.h"

class UDRThrowableItemDefinition;

/** 기존 투척 아이템의 설정을 재사용하는 캐릭터 스킬 정의 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRThrowSkillDefinition : public UDRSkillDefinition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** 스킬이 투척할 투사체, 효과, 연출과 애니메이션을 제공하는 기존 아이템 정의 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Throw")
	TObjectPtr<UDRThrowableItemDefinition> ThrowableDefinition;
};
