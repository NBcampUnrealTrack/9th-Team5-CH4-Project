#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DRSkillSlotViewModel.generated.h"

class ADRPlayerCharacter;
class UAbilitySystemComponent;
class UDRGA_StackedSpearThrowSkill;
class UDRSkillComponent;
class UDRSkillDefinition;
class UTexture2D;

/** HUD의 단일 스킬 슬롯 표시 값을 제공한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRSkillSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(
		ADRPlayerCharacter* InPlayerCharacter,
		EDRSkillSlot InSkillSlot);
	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	FLinearColor IconTint = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	FText InputKeyText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	FText CooldownText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	float CooldownRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	bool IsOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	FText StackText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	bool IsStackVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	bool IsVisible = false;

private:
	UFUNCTION()
	void HandleSkillChanged();

	void HandleStackChanged();
	void HandleCooldownTagChanged(FGameplayTag, int32);
	void RefreshSkill();
	void RefreshInputKey();
	void RefreshCooldown();
	void RefreshStack();
	void UpdateStackAbility(const UDRSkillDefinition* SkillDefinition);
	void UpdateCooldownTag(FGameplayTag NewCooldownTag);
	void StopCooldownTimer();

	TWeakObjectPtr<ADRPlayerCharacter> PlayerCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UDRGA_StackedSpearThrowSkill> StackedSpearAbility;
	TWeakObjectPtr<UDRSkillComponent> SkillComponent;
	EDRSkillSlot SkillSlot = EDRSkillSlot::Count;
	FGameplayTag CooldownTag;
	FDelegateHandle CooldownTagChangedHandle;
	FTimerHandle CooldownTimerHandle;
};
