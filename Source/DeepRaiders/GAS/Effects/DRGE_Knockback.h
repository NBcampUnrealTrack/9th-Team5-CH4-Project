#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_Knockback.generated.h"

/** SetByCaller 거리와 EffectContext Origin을 사용해 대상에게 일회성 넉백을 요청한다. */
UCLASS()
class DEEPRAIDERS_API UDRGE_Knockback : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDRGE_Knockback(const FObjectInitializer& ObjectInitializer);
};
