#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRStartingWeaponTable.generated.h"

class UDRItemDefinition;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRStartingWeaponTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starting Weapon")
	TSoftObjectPtr<UDRItemDefinition> WeaponDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starting Weapon")
	FText Description;
};
