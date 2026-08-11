#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRMiningTypes.h"
#include "DRMiningItemDefinition.generated.h"

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRMiningItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mining", meta = (ShowOnlyInnerProperties))
	FDRMiningSettings MiningSettings;
};
