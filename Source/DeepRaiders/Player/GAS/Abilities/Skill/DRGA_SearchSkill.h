#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_SearchSkill.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRGA_SearchSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool IsReplicateEndAbility,
		bool IsWasCancelled) override;

	UFUNCTION()
	void HandleSearchFinished();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Search", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float SearchRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Search", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float RevealDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Search", meta = (ClampMin = "0", ClampMax = "255", UIMin = "0", UIMax = "255"))
	int32 StencilValue = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Search|VFX")
	bool IsVFXVisibleToAll = true;

private:
	void PlaySearchVFX(ADRPlayerCharacter* Character) const;
	void RevealEnemies(const ADRPlayerCharacter* Character, bool IsSharedReveal);
	void StopReveals(bool IsSharedReveal);
	bool HasTeamSharePerk(const FGameplayAbilityActorInfo* ActorInfo) const;

	FGuid LocalRevealId;
	FGuid SharedRevealId;
	TArray<TWeakObjectPtr<ADRPlayerCharacter>> LocalRevealedCharacters;
	TArray<TWeakObjectPtr<ADRPlayerCharacter>> SharedRevealedCharacters;
};
