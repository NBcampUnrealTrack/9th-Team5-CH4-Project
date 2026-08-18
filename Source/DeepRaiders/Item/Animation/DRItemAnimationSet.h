#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRItemAnimationSet.generated.h"

class UAnimSequenceBase;
class UAnimMontage;

UENUM(BlueprintType)
enum class EDRLocomotionStyle : uint8
{
	Unarmed,
	Rifle
};

/**
 * 아이템 장착 시 사용할 캐릭터 애니메이션 프로필.
 *
 * ItemDefinition은 이 Asset을 참조하기만 하고,
 * 실제 재생/블렌딩은 Character AnimBP / GameplayAbility가 담당한다.
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRItemAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion")
	EDRLocomotionStyle LocomotionStyle = EDRLocomotionStyle::Unarmed;
	
	/** 아이템 장착 중 사용할 기본 자세 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Pose")
	TObjectPtr<UAnimSequenceBase> AimIdle;

	/** 아이템의 PrimaryAction 실행 시 사용할 Montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Action")
	TObjectPtr<UAnimMontage> PrimaryActionMontage;
};
