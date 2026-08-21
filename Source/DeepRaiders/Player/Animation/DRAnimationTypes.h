#pragma once

#include "CoreMinimal.h"
#include "DRAnimationTypes.generated.h"

UENUM(BlueprintType)
enum class EDRCardinalDirection : uint8
{
	Forward,
	Right,
	Backward,
	Left
};