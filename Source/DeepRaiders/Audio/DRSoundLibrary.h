#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DRSoundLibrary.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundConcurrency;

UENUM(BlueprintType)
enum class EDRSoundPlaybackMode : uint8
{
	TwoDimensional_2D,
	AttachedToSource_3D,
	WorldLocation_3D
};

UENUM(BlueprintType)
enum class EDRSoundAudience : uint8
{
	CueRecipients,
	InstigatorOnly
};

USTRUCT(BlueprintType)
struct FDRSoundDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	EDRSoundPlaybackMode PlaybackMode = EDRSoundPlaybackMode::AttachedToSource_3D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	EDRSoundAudience Audience = EDRSoundAudience::CueRecipients;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundConcurrency> Concurrency;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundClass> SoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float DefaultPriority = 1.0f;

	/** 필수 알림음처럼 Concurrency 제한을 적용하지 않을 때 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool bAlwaysPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	/** 실제 반복 재생은 Sound Cue나 MetaSound 에셋에서 설정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float FadeOutTime = 0.1f;
};

/** GameplayCue 태그별 로컬 사운드 재생 정책을 보관한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRSoundLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FDRSoundDefinition* FindDefinition(const FGameplayTag& SoundTag) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "GameplayCue"))
	TMap<FGameplayTag, FDRSoundDefinition> Definitions;
};
