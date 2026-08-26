#pragma once

#include "CoreMinimal.h"
#include "DRSkillTypes.generated.h"

UENUM(BlueprintType)
enum class EDRSkillSlot : uint8
{
	One,
	Two,
	Count UMETA(Hidden)
};
