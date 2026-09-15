#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRTitleMapDefinition.generated.h"

class UTexture2D;
class UWorld;

USTRUCT(BlueprintType)
struct FDRTitleMapDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<UWorld> Map;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<UTexture2D> PreviewImage;
};
