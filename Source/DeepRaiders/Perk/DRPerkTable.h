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

	/** 설정된 랭크 데이터 개수를 최대 랭크로 반환한다. */
	int32 GetMaxRank() const
	{
		return Ranks.Num();
	}

	/** 요청한 랭크에 대응하는 데이터와 GameplayEffect가 유효한지 확인한다. */
	bool IsValidRank(int32 Rank) const
	{
		return Rank > 0
			&& Ranks.IsValidIndex(Rank - 1)
			&& EffectClass;
	}

	/** 요청한 랭크 데이터를 반환하며 유효하지 않으면 nullptr을 반환한다. */
	const FDRPerkRankData* GetRankData(int32 Rank) const
	{
		return IsValidRank(Rank) ? &Ranks[Rank - 1] : nullptr;
	}
};
