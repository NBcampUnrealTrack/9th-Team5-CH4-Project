#pragma once

#include "CoreMinimal.h"
#include "DRGA_SpearThrowSkill.h"
#include "DRGA_StackedSpearThrowSkill.generated.h"

DECLARE_MULTICAST_DELEGATE(FDRSpearStackChangedSignature);

/** 최대 스택을 저장하고 일정 간격으로 사용하는 창던지기 강화 스킬이다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_StackedSpearThrowSkill : public UDRGA_SpearThrowSkill
{
	GENERATED_BODY()

public:
	UDRGA_StackedSpearThrowSkill();
	int32 GetCurrentStackCount() const;
	int32 GetMaximumStackCount() const;
	FDRSpearStackChangedSignature& GetStackChangedDelegate();

protected:
	virtual void OnGiveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;
	virtual bool CheckCooldown(
		const FGameplayAbilitySpecHandle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	void BindRechargeCooldown(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec);
	void UnbindRechargeCooldown();
	void HandleRechargeCooldownTagChanged(FGameplayTag, int32 NewCount);
	void StartRechargeCooldown();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw|Stack", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumStackCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Spear Throw|Stack", meta = (ClampMin = "0.0", Units = "s"))
	float FireInterval = 0.5f;

private:
	mutable int32 CurrentStackCount = 1;
	float RechargeCooldownDuration = 0.f;
	FGameplayTag RechargeCooldownTag;
	TWeakObjectPtr<UAbilitySystemComponent> RechargeAbilitySystemComponent;
	FDelegateHandle RechargeCooldownTagChangedHandle;
	mutable FDRSpearStackChangedSignature StackChangedDelegate;
};
