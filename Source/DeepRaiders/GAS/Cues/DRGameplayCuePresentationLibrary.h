#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "DRGameplayCuePresentationLibrary.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGameplayCuePresentationLibrary
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameplayCue|Presentation")
	static void ExecuteLocalSoundCue(
		AActor* Target,
		FGameplayTag SoundCueTag,
		const FGameplayCueParameters& SourceParameters);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue|Presentation")
	static void ActivateLocalSoundCue(
		AActor* Target,
		FGameplayTag SoundCueTag,
		const FGameplayCueParameters& SourceParameters);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue|Presentation")
	static void RemoveLocalSoundCue(
		AActor* Target,
		FGameplayTag SoundCueTag,
		const FGameplayCueParameters& SourceParameters);

private:
	static void HandleLocalSoundCue(
		AActor* Target,
		const FGameplayTag& SoundCueTag,
		const FGameplayCueParameters& SourceParameters,
		EGameplayCueEvent::Type EventType);
};