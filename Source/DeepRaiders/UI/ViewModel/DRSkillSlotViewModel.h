#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DRSkillSlotViewModel.generated.h"

class ADRPlayerCharacter;
class UAbilitySystemComponent;
class UDRSkillComponent;
class UDRPerkComponent;
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
	int32 CurrentCharges = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	int32 MaxCharges = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	bool UsesCharges = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	bool IsVisible = false;

private:
	UFUNCTION()
	void HandleSkillChanged();
	UFUNCTION()
	void HandlePerksChanged();

	void HandleCooldownTagChanged(FGameplayTag Tag, int32 NewCount);
	void RefreshSkill();
	void RefreshInputKey();
	void RefreshCooldown();
	void UpdateCooldownTag(FGameplayTag NewCooldownTag);
	void StopCooldownTimer();

	TWeakObjectPtr<ADRPlayerCharacter> PlayerCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UDRSkillComponent> SkillComponent;
	TWeakObjectPtr<UDRPerkComponent> PerkComponent;
	EDRSkillSlot SkillSlot = EDRSkillSlot::Count;
	FGameplayTag CooldownTag;
	FDelegateHandle CooldownTagChangedHandle;
	FTimerHandle CooldownTimerHandle;
};
