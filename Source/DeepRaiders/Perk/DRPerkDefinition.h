#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRPerkDefinition.generated.h"

UCLASS(BlueprintType, AutoExpandCategories = ("Perk"))
class DEEPRAIDERS_API UDRPerkDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRPerkDefinition();
};
