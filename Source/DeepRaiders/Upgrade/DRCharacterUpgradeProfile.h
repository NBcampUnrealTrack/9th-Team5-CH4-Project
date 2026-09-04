#pragma once

#include "CoreMinimal.h"
#include "DRUpgradeProfile.h"
#include "DRCharacterUpgradeProfile.generated.h"

class UGameplayEffect;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRCharacterUpgradeProfile : public UDRUpgradeProfile
{
	GENERATED_BODY()

public:
	UDRCharacterUpgradeProfile();
	bool IsUsable() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSubclassOf<UGameplayEffect> EffectClass;

private:
	void AddStat(FGameplayTag UpgradeTag, FGameplayTag ValueTag, const FText& DisplayName);
};
