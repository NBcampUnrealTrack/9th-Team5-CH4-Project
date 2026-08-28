#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRWeaponPresentationTypes.h"
#include "DRSprayerWeaponDefinition.generated.h"

class UGameplayEffect;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRSprayerWeaponDefinition : public UDRItemDefinition
{
	GENERATED_BODY()

public:
	UDRSprayerWeaponDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Snow", meta = ( ClampMin = "0.0", UIMin = "0.0"))
	float SnowCostPerSecond = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Snow")
	TSubclassOf<UGameplayEffect> SnowCostEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation")
	FDRWeaponPresentationData ActivePresentation;
};
