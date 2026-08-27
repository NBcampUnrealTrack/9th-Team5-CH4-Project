#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DRGameplayEffectData.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRGameplayEffectData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effect")
	TSubclassOf<UGameplayEffect> EffectClass = nullptr;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Gameplay Effect",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EffectLevel = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effect")
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
};