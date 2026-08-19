#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "DRGameplayCueSound.generated.h"

class UDRSoundLibrary;

/** GameplayCue 이벤트를 SoundSubsystem 요청으로 변환하는 공통 Cue 베이스다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API UDRGameplayCueSound : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* Target,
		const FGameplayCueParameters& Parameters) const override;
	virtual bool OnActive_Implementation(AActor* Target,
		const FGameplayCueParameters& Parameters) const override;
	virtual bool WhileActive_Implementation(AActor* Target,
		const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* Target,
		const FGameplayCueParameters& Parameters) const override;

protected:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Sound")
	float ResolvePriority(AActor* Target, const FGameplayCueParameters& Parameters) const;
	virtual float ResolvePriority_Implementation(AActor* Target,
		const FGameplayCueParameters& Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Sound")
	AActor* ResolveSourceActor(AActor* Target, const FGameplayCueParameters& Parameters) const;
	virtual AActor* ResolveSourceActor_Implementation(AActor* Target,
		const FGameplayCueParameters& Parameters) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<UDRSoundLibrary> SoundLibrary;

private:
	FGameplayTag ResolveSoundTag(const FGameplayCueParameters& Parameters) const;
	bool RequestSound(AActor* Target, const FGameplayCueParameters& Parameters, bool bLoopEvent) const;
};
