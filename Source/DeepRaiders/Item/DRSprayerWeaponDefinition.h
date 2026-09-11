#pragma once

#include "CoreMinimal.h"
#include "DRRangedWeaponDefinition.h"
#include "DRWeaponPresentationTypes.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "GameplayTagContainer.h"
#include "DRSprayerWeaponDefinition.generated.h"

class UGameplayEffect;
class UDRWeaponUpgradeProfile;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSprayerWeaponDataTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SnowCostPerSecond = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SprayTickInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SprayRange = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SprayHalfAngleDegrees = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SprayOriginForwardOffset = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SprayOriginHeightOffset = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HitReactionInterval = 0.5f;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRSprayerWeaponDefinition : public UDRRangedWeaponDefinition
{
	GENERATED_BODY()

public:
	UDRSprayerWeaponDefinition();

	virtual UDRWeaponUpgradeProfile* GetUpgradeProfile() const override { return UpgradeProfile.Get(); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	TObjectPtr<UDRWeaponUpgradeProfile> UpgradeProfile = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Snow", meta = ( ClampMin = "0.0", UIMin = "0.0"))
	float SnowCostPerSecond = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Resource|Snow")
	TSubclassOf<UGameplayEffect> SnowCostEffectClass;
	
	// ================================
	// Config
	// ================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Tick", meta = ( AllowPrivateAccess, ClampMin = "0.02", UIMin = "0.02", Units = "s"))
	float SprayTickInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Range", meta = ( AllowPrivateAccess, ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float SprayRange = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Range", meta = ( AllowPrivateAccess, ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0", Units = "deg"))
	float SprayHalfAngleDegrees = 20.f;

	/*
	 * Gameplay 판정용 Origin.
	 * 애니메이션 / Weapon Socket에 의존하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Origin", meta = ( AllowPrivateAccess, Units = "cm"))
	float SprayOriginForwardOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Origin", meta = ( AllowPrivateAccess, Units = "cm"))
	float SprayOriginHeightOffset = 60.f;
	
	/*
	* 권장 순서:
	* [0] FrozenDamage
	* [1] FreezeGain
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Effect", meta = (AllowPrivateAccess))
	TArray<FDRGameplayEffectData> ImpactEffects;

	// 지속형 Spray는 매 Tick마다 HitReaction을 재생하지 않는다.
	// 실제 Damage가 발생한 대상에 대해 일정 간격으로만 피격 반응을 실행한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Hit Reaction", meta = (AllowPrivateAccess, ClampMin = "0.0", Units = "s"))
	float HitReactionInterval = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprayer|Presentation")
	FGameplayTag SprayGameplayCueTag;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation")
	FDRWeaponPresentationData ActivePresentation;
	
	UPROPERTY(EditDefaultsOnly, Category = "Sprayer|Presentation")
	TSubclassOf<UGameplayEffect> ActivePresentationEffectClass;

};
