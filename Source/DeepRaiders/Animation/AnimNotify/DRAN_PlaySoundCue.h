#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "DRAN_PlaySoundCue.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRAN_PlaySoundCue : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag SoundCueTag;

	/** 비어 있으면 SkeletalMeshComponent 위치를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	FName LocationSocketName = NAME_None;
};
