#pragma once

#include "CoreMinimal.h"
#include "DRItemActionTypes.generated.h"

UENUM(BlueprintType)
enum class EDRItemActionType : uint8
{
	None UMETA(DisplayName = "None"),

	/** 지형 채굴 */
	Dig UMETA(DisplayName = "Dig"),

	/** 근접 공격 */
	MeleeAttack UMETA(DisplayName = "Melee Attack"),

	/** 투척 */
	Throw UMETA(DisplayName = "Throw")
};