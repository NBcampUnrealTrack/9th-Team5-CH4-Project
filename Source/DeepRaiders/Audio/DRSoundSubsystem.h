#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRSoundSubsystem.generated.h"

class AActor;
class UAudioComponent;
class UDRSoundLibrary;
struct FDRSoundDefinition;

USTRUCT(BlueprintType)
struct FDRSoundRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound",
		meta = (Categories = "GameplayCue"))
	FGameplayTag SoundTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TObjectPtr<AActor> Instigator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	FVector WorldLocation = FVector::ZeroVector;

	/** 음수이면 DataAsset의 기본 우선순위를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float PriorityOverride = -1.0f;
};

/** 각 클라이언트 월드에서 GameplayCue 사운드를 재생하고 루프 수명을 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRSoundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Sound")
	UAudioComponent* RequestSound(UDRSoundLibrary* Library, const FDRSoundRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void StopSound(UDRSoundLibrary* Library, AActor* SourceActor, FGameplayTag SoundTag);

private:
	struct FLoopKey
	{
		TWeakObjectPtr<AActor> SourceActor;
		FGameplayTag SoundTag;

		bool operator==(const FLoopKey& Other) const
		{
			return SourceActor == Other.SourceActor && SoundTag == Other.SoundTag;
		}

		friend uint32 GetTypeHash(const FLoopKey& Key)
		{
			return HashCombine(GetTypeHash(Key.SourceActor), GetTypeHash(Key.SoundTag));
		}
	};

	UAudioComponent* CreateAudioComponent(const FDRSoundDefinition& Definition,
		const FDRSoundRequest& Request, float Priority) const;
	bool ShouldPlayForLocalClient(const FDRSoundDefinition& Definition, AActor* Instigator) const;
	void HandleAudioFinished(UAudioComponent* AudioComponent);

	TMap<FLoopKey, TWeakObjectPtr<UAudioComponent>> ActiveLoops;
};
