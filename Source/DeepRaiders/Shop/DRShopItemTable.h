#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRShopItemTable.generated.h"

class UDRItemDefinition;

USTRUCT(BlueprintType)
struct FDRShopItemTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDRItemDefinition> ItemDefinition;
};
