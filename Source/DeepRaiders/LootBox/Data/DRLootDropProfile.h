#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DeepRaiders/LootBox/DRLootTypes.h"
#include "DRLootDropProfile.generated.h"

class UDataTable;

/*
 * 여러 LootDropComponent가 공유하는 정적 전리품 설정
 * 실제 추첨과 월드 스폰은 LootDropComponent가 담당.
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRLootDropProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()
    
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    TObjectPtr<UDataTable> LootTable = nullptr;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    TMap<EDRLootTier, FDRLootTierConfig> TierConfigs;
};
