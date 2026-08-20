#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRItemAnimationSet.generated.h"

class UAnimSequenceBase;
class UAnimMontage;
class UAnimInstance;

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
};
