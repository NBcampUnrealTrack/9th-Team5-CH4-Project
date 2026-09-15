#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_BlinkRecovery.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGE_BlinkRecovery : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDRGE_BlinkRecovery(const FObjectInitializer& ObjectInitializer);
};
