#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRStartingWeaponTable.generated.h"

class UDRProjectileWeaponItemDefinition;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRStartingWeaponTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starting Weapon")
	TSoftObjectPtr<UDRProjectileWeaponItemDefinition> WeaponDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starting Weapon")
	FText Description;
};
