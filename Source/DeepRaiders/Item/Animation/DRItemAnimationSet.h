#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRItemAnimationSet.generated.h"

class UAnimSequenceBase;
class UAnimMontage;

/**
 * 아이템 장착 시 사용할 캐릭터 애니메이션 프로필.
 *
 * ItemDefinition은 이 Asset을 참조하기만 하고,
 * 실제 재생/블렌딩은 Character AnimBP가 담당한다.
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRItemAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 해당 아이템을 들고 있을 때 사용하는
	 * 기본 상체 자세.
	 *
	 * ex)
	 * Rifle  -> MF_Rifle_Idle_ADS
	 * Pistol -> Pistol Idle
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Pose")
	TObjectPtr<UAnimSequenceBase> AimIdle;

	/** 기본 공격 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Action")
	TObjectPtr<UAnimMontage> FireMontage;

	/** 재장전 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Action")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** 장착 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Action")
	TObjectPtr<UAnimMontage> EquipMontage;

	/** 탄약 부족 등의 헛발 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Action")
	TObjectPtr<UAnimMontage> DryFireMontage;
};
