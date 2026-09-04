#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_CharacterStatUpgrade.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGE_CharacterStatUpgrade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDRGE_CharacterStatUpgrade();

private:
	void AddStat(const FGameplayAttribute& Attribute, FGameplayTag ValueTag);
};
