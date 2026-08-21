#pragma once

#include "CoreMinimal.h"
#include "DRItemTypes.generated.h"


UENUM(BlueprintType)
enum class EDRItemCategory : uint8
{
	Ore,
	Equipment,
	Consumable,
	Perk,
	End,
};

// ItemInstance에 부여될 개별 인스턴스 값
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRItemRuntimeState
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRProjectileWeaponRuntimeState : public FDRItemRuntimeState
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Runtime")
	int32 CurrentAmmo = 0;
};

// 현재 기획에선 필요 없지만 고급 근접 무기의 제공 가능성 고려
// USTRUCT(BlueprintType)
// struct DEEPRAIDERS_API FDRMeleeWeaponRuntimeState : public FDRItemRuntimeState
// {
// 	GENERATED_BODY()
// 	
// 	// 근접 무기 내구도
// 
// 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Runtime")
// 	float CurrentDurability = 0.f;
// };


