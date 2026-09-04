#pragma once

#include "CoreMinimal.h"
#include "DRUpgradeProfile.h"
#include "DRUpgradeTypes.h"
#include "DRCharacterUpgradeProfile.generated.h"

class UGameplayEffect;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRCharacterUpgradeProfile : public UDRUpgradeProfile
{
	GENERATED_BODY()

public:
	UDRCharacterUpgradeProfile();

	const FDRStatUpgradeData* FindStatUpgrade(FGameplayTag UpgradeTag) const;

	const TArray<FDRStatUpgradeData>& GetStatUpgrades() const
	{
		return StatUpgrades;
	}

	virtual bool IsUsable() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSubclassOf<UGameplayEffect> EffectClass;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TArray<FDRStatUpgradeData> StatUpgrades;

private:
	void AddStat(FGameplayTag UpgradeTag, FGameplayTag ValueTag, const FText& DisplayName);
};
