#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DRItemTypes.generated.h"

UENUM(BlueprintType)
enum class EDRItemCategory : uint8
{
	Ore,
	Equipment,
	Consumable,
	Perk,
	Skill,
	Pickup,
	End,
};

UENUM(BlueprintType)
enum class EDRItemRarity : uint8
{
	Common UMETA(DisplayName = "Common"),
	Uncommon UMETA(DisplayName = "Uncommon"),
	Rare UMETA(DisplayName = "Rare"),
	Epic UMETA(DisplayName = "Epic"),
	Legendary UMETA(DisplayName = "Legendary"),
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

/**
 * 눈을 소모하는 Projectile Weapon의 인스턴스별 업그레이드 상태.
 *
 * 업그레이드 결과값이 아니라 스탯별 현재 레벨을 영구 상태로 보관한다.
 * 레벨에 대응하는 실제 수치는 UDRWeaponUpgradeProfile에서 조회한다.
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowProjectileWeaponRuntimeState : public FDRItemRuntimeState
{
	GENERATED_BODY()

public:
	int32 GetUpgradeLevel(const FGameplayTag& UpgradeTag) const
	{
		const int32* FoundLevel = UpgradeLevels.Find(UpgradeTag);
		return FoundLevel != nullptr ? FMath::Max(0, *FoundLevel) : 0;
	}

	bool SetUpgradeLevel(const FGameplayTag& UpgradeTag, int32 NewLevel)
	{
		if (!UpgradeTag.IsValid() || NewLevel < 0)
		{
			return false;
		}

		if (NewLevel == 0)
		{
			UpgradeLevels.Remove(UpgradeTag);
		}
		else
		{
			UpgradeLevels.FindOrAdd(UpgradeTag) = NewLevel;
		}

		return true;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Upgrade")
	TMap<FGameplayTag, int32> UpgradeLevels;
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


