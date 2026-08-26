#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_CombatRollSkill.generated.h"

class UAnimMontage;

UCLASS()
class DEEPRAIDERS_API UDRGA_CombatRollSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void HandleRollFinished();

	UFUNCTION()
	void HandleRollInterrupted();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll")
	TObjectPtr<UAnimMontage> RollMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float RollDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float RollPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName FrontRollSection = TEXT("Roll_Front");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName BackRollSection = TEXT("Roll_Back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName LeftRollSection = TEXT("Roll_Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName RightRollSection = TEXT("Roll_Right");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName FrontLeftRollSection = TEXT("Roll_Front_Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName FrontRightRollSection = TEXT("Roll_Front_Right");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName BackLeftRollSection = TEXT("Roll_Back_Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat Roll|Sections")
	FName BackRightRollSection = TEXT("Roll_Back_Right");

private:
	FVector ResolveRollDirection(const ADRPlayerCharacter* Character) const;
	FName ResolveRollSection(const ADRPlayerCharacter* Character, const FVector& RollDirection) const;
};
