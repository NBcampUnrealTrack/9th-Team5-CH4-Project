#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_VoxelContained.generated.h"

/** 매몰 상태를 GAS 태그로 복제하는 무한 지속 GameplayEffect. */
UCLASS()
class DEEPRAIDERS_API UDRGE_VoxelContained : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDRGE_VoxelContained(const FObjectInitializer& ObjectInitializer);
};
