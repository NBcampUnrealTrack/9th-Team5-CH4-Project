#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRUpgradeProfile.generated.h"

UCLASS(Abstract, BlueprintType)
class DEEPRAIDERS_API UDRUpgradeProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual bool IsUsable() const PURE_VIRTUAL(UDRUpgradeProfile::IsUsable, return false;);
};
