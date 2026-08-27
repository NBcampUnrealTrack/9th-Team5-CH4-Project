#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DREffectPickupItemDefinition.generated.h"

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDREffectPickupItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()
	
public:
	UDREffectPickupItemDefinition();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Pickup")
	TArray<FDRGameplayEffectData> PickupEffects;	
};
