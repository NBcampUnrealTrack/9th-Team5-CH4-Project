#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "GameplayEffectTypes.h"
#include "DRGA_SuperJumpSkill.generated.h"

class UAbilitySystemComponent;
class UDRCharacterMovementComponent;
class ADRPlayerCharacter;

UCLASS()
class DEEPRAIDERS_API UDRGA_SuperJumpSkill : public UDRGA_CharacterSkillBase
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

	/** 이동 컴포넌트가 알린 실제 착지에서 보관 중인 모든 Spec을 처리한다. */
	void HandleCharacterLanded(const FHitResult& Hit);
	void ApplyLandingEffects();
	void ClearPendingLandingEffects();

	/** 현재 중력 기준으로 도달할 최고 높이. 초기 수직 속도는 런타임에 계산한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Super Jump", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float JumpHeight = 735.0f;

	/** 슈퍼점프 중의 공중 방향 전환력. 지상 이동과 분리해 조절한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Super Jump", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float AirControl = 0.25f;

	/** 슈퍼점프 중 지상 최대 이동속도에 적용할 수평 속도 비율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Super Jump", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MaxAirSpeedMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Super Jump")
	bool bAllowInAir = false;

	/** 히어로 랜딩의 완성된 Spec들을 전달할 적 탐색 반경이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Super Jump|Landing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float LandingEffectRadius = 400.f;

private:
	TArray<FGameplayEffectSpecHandle> PendingLandingEffectSpecs;
	TWeakObjectPtr<UAbilitySystemComponent> LandingSourceAbilitySystem;
	TWeakObjectPtr<ADRPlayerCharacter> LandingSourceCharacter;
	TWeakObjectPtr<UDRCharacterMovementComponent> LandingMovementComponent;
	uint32 PendingSuperJumpSequence = 0;
};
