#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_CharacterSkillBase.generated.h"

class ADRPlayerCharacter;
class ADRPlayerState;

UCLASS(Abstract)
class DEEPRAIDERS_API UDRGA_CharacterSkillBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDRGA_CharacterSkillBase();

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	ADRPlayerCharacter* GetPlayerCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
	ADRPlayerState* GetDRPlayerState(const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Skill|Cooldown",
		meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float CooldownDuration = 1.0f;
};
