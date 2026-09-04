#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_WeaponStatUpgrade.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGE_WeaponStatUpgrade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDRGE_WeaponStatUpgrade();

private:
	void AddStat(const FGameplayAttribute& Attribute, FGameplayTag ValueTag);
};
