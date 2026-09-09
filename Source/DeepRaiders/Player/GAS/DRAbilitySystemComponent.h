#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DRAbilitySystemComponent.generated.h"

struct FActiveGameplayEffect;
struct FActiveGameplayCueContainer;

/**
 * PlayerState가 소유하는 프로젝트 ASC.
 * Avatar 교체 시 지속형 GameplayCue를 복원하고, Mixed 복제에서 중복된 VFX Cue의 수명을 보정한다.
 */
UCLASS()
class DEEPRAIDERS_API UDRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleGameplayEffectRemoved(const FActiveGameplayEffect& RemovedEffect);
	const FActiveGameplayEffect* FindRemainingEffectWithCue(const FGameplayTag& GameplayCueTag) const;
	void ReplayGameplayCueContainer(const FActiveGameplayCueContainer& GameplayCueContainer);
	void ReplayActiveGameplayEffectCues();
	bool ShouldUseActiveGameplayEffectsForCueReplay() const;

	FDelegateHandle GameplayEffectRemovedDelegateHandle;
};
