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

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowAbsorbPresentationData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> BeamVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation",
		meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag StartSoundCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation",
		meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag LoopSoundCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation",
		meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag EndSoundCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Gain Pulse",
		meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag GainPulseSoundCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Gain Pulse",
		meta = (ClampMin = "1.0", UIMin = "1.0"))
	float GainPulseGaugeIntervalMin = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Gain Pulse",
		meta = (ClampMin = "1.0", UIMin = "1.0"))
	float GainPulseGaugeIntervalMax = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FName AttachSocketName = TEXT("VFXPoint");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Niagara")
	FName ScaleParameterName = TEXT("User.Scale");

};
