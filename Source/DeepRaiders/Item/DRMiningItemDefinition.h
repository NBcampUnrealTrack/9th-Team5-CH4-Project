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
	/** 장착 시 DRMiningComponent에 적용할 채굴 전용 수치다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mining", meta = (ShowOnlyInnerProperties))
	FDRMiningSettings MiningSettings;
};
