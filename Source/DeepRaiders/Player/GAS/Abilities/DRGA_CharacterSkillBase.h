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

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context) const override;
#endif

protected:
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

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

#if WITH_EDITOR
	static bool IsCooldownEffectValid(
		const UGameplayEffect* CooldownEffect,
		FGameplayTag ExpectedCooldownTag,
		FDataValidationContext& Context);
#endif

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Skill|Cooldown",
		meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float CooldownDuration = 1.0f;
};
