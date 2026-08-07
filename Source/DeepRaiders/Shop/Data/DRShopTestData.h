#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRShopTestData.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FDRShopItemData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Item")
	FText ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Item", meta = (ClampMin = "0"))
	int32 Price = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Item", meta = (MultiLine = true))
	FText Information;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRShopTestData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<FDRShopItemData> Items;
};
