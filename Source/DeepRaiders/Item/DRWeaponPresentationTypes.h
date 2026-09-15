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
struct DEEPRAIDERS_API FDRBeamVFXPresentationData
{
	GENERATED_BODY()

	/** FirePresentation과 같은 시점에 추가로 실행할 선택적 Beam VFX. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> VFX = nullptr;

	/** Niagara Position 타입의 시작점 User Parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Niagara")
	FName StartPositionParameterName = TEXT("User.StartPosition");

	/** Niagara Position 타입의 끝점 User Parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Niagara")
	FName EndPositionParameterName = TEXT("User.EndPosition");
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
