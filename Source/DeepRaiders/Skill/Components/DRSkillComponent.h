#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "DRSkillComponent.generated.h"

class UDRSkillDefinition;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRSkillChangedSignature);

/** 플레이어의 스킬 슬롯과 GAS 부여 상태를 관리한다. */
UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSkillComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UDRSkillDefinition* GetCurrentSkill(EDRSkillSlot SkillSlot) const;

	bool CanEquipSkill(const UDRSkillDefinition* SkillDefinition) const;
	bool EquipSkill(UDRSkillDefinition* SkillDefinition);
	void GrantDefaultSkills();

	UPROPERTY(BlueprintAssignable, Category = "Skill")
	FDRSkillChangedSignature OnSkillChanged;

private:
	void RemovePreviousSkillAbility(
		UAbilitySystemComponent* AbilitySystemComponent,
		int32 SlotIndex);

	static int32 GetSkillInputId(EDRSkillSlot SkillSlot);
	static int32 GetSkillSlotIndex(EDRSkillSlot SkillSlot);

	UFUNCTION()
	void OnRep_EquippedSkills();

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_EquippedSkills,
		Category = "Skill",
		meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UDRSkillDefinition>> EquippedSkills;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Default")
	TArray<TObjectPtr<UDRSkillDefinition>> DefaultSkills;

	TArray<FGameplayAbilitySpecHandle> SkillAbilityHandles;
};
