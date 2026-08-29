#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRItemAnimationSet.generated.h"

class UAnimMontage;
class UAnimInstance;
class UDRHitReactionSet;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRItemAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Layer")
	TSubclassOf<UAnimInstance> AnimLayerClass;

	/** 아이템의 PrimaryAction 실행 시 사용할 Montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Action")
	TObjectPtr<UAnimMontage> PrimaryActionMontage;

	/** 현재 장착 자세에 대응하는 피격 애니메이션 세트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Hit")
	TObjectPtr<UDRHitReactionSet> HitReactionSet;
};