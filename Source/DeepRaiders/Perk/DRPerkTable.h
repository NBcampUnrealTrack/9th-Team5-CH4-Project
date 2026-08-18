#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "DRPerkTable.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FDRPerkRankData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk", meta = (ClampMin = "0"))
	int32 Price = 0;
};

USTRUCT(BlueprintType)
struct FDRPerkTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk")
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk")
	TArray<FDRPerkRankData> Ranks;

	int32 GetMaxRank() const
	{
		return Ranks.Num();
	}

	bool IsValidRank(int32 Rank) const
	{
		return Rank > 0
			&& Ranks.IsValidIndex(Rank - 1)
			&& EffectClass;
	}

	const FDRPerkRankData* GetRankData(int32 Rank) const
	{
		return IsValidRank(Rank) ? &Ranks[Rank - 1] : nullptr;
	}
};
