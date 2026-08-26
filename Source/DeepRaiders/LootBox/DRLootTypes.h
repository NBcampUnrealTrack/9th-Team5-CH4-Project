#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DeepRaiders/Item/DRItemTypes.h"
#include "DRLootTypes.generated.h"

class UDRItemDefinition;

UENUM(BlueprintType)
enum class EDRLootTier : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary,
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRLootTableRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDRItemDefinition> ItemDefinition = nullptr;
	
	// 동일 희귀도 안에서 이 아이템이 선택될 상대 가중치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClmapMin = "0.0"))
	float SelectionWeight = 1.f;
	
	// MaxStack을 넘는 개수는 새로운 Stack으로 등장
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinQuantity = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxQuantity = 1;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRLootTierConfig
{
	GENERATED_BODY()
	
	// 아이템 등장 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (Clampmin = "0"))
	int32 MinDropCount = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (Clampmin = "0"))
	int32 MaxDropCount = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TMap<EDRItemRarity, float> RarityWeights;
};
