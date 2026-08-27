#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRPerkDefinition.generated.h"

class UGameplayEffect;

UCLASS(BlueprintType, AutoExpandCategories = ("Perk"))
class DEEPRAIDERS_API UDRPerkDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRPerkDefinition();

	/** GameplayEffect의 SetByCaller에 전달할 시트 기반 퍽 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Balance")
	TMap<FGameplayTag, float> EffectValues;

	/** 퍽 획득 시 직접 적용할 GameplayEffect다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|GAS")
	TSubclassOf<UGameplayEffect> PerkEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|GAS")
	uint8 bPersistThroughDeath:1 = true;
};
