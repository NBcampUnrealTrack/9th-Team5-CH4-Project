#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRStartingSkillTable.generated.h"

class UDRSkillDefinition;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRStartingSkillTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starting Skill")
	TSoftObjectPtr<UDRSkillDefinition> SkillDefinition;
};
