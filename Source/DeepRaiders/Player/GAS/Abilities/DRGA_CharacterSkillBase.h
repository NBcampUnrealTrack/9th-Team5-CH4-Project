#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_CharacterSkillBase.generated.h"

class ADRPlayerCharacter;
class ADRPlayerState;
class UDRSkillDefinition;

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
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

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

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	ADRPlayerCharacter* GetPlayerCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
	const UDRSkillDefinition* GetCurrentSkillDefinition() const;
	FGameplayTag GetCooldownTag() const;

	/** 스킬 Commit 성공 뒤, 해당 스킬에 장착된 퍽의 사용 시 효과를 실행한다. */
	void NotifySkillCommitted(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 스킬 기본 효과와 이 스킬에 장착된 퍽의 활성 중 효과를 적용한다. */
	void NotifySkillActivated(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

#if WITH_EDITOR
	static bool IsCooldownEffectValid(
		const UGameplayEffect* CooldownEffect,
		FDataValidationContext& Context);
#endif

	mutable FGameplayTagContainer CurrentCooldownTags;

	/** 현재 스킬 활성 동안 유지되며 EndAbility에서 회수할 GE 핸들이다. */
	mutable TArray<FActiveGameplayEffectHandle> ActiveSkillEffectHandles;
};
