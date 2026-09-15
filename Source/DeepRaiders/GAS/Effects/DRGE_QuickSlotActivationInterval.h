#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_QuickSlotActivationInterval.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGE_QuickSlotActivationInterval : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDRGE_QuickSlotActivationInterval(const FObjectInitializer& ObjectInitializer);
};