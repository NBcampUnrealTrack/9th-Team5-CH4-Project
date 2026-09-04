#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRUpgradeTypes.h"
#include "DRUpgradeProfile.generated.h"

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRUpgradeProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	const FDRStatUpgradeData* FindStatUpgrade(FGameplayTag UpgradeTag) const;

	const TArray<FDRStatUpgradeData>& GetStatUpgrades() const
	{
		return StatUpgrades;
	}

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TArray<FDRStatUpgradeData> StatUpgrades;
};
