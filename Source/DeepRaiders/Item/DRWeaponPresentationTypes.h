#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DRWeaponPresentationTypes.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRWeaponPresentationData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> VFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag SoundCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FName AttachSocketName = TEXT("VFXPoint");
};