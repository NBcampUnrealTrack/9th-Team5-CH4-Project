#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTypes.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DeepRaiders/Skill/Turret/DRTurret.h"
#include "DRGA_TurretSkill.generated.h"

class ADRPlacementPreviewActor;
class ADRPlacementTargetActor;
class ADRTurret;
class UAbilityTask_WaitTargetData;

/** 설치 영역을 조준한 뒤 공격키(Primary)로 포탑을 설치하는 스킬이다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_TurretSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_TurretSkill();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Turret", meta = (ShowOnlyInnerProperties))
	FDRPlacementSettings PlacementSettings;

	/** 프리뷰 영역과 설치 높이에 사용하는 포탑의 전체 크기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Turret", meta = (ClampMin = "1.0", Units = "cm"))
	FVector TurretDimensions = FVector(180.f, 180.f, 220.f);

	/** 0이면 파괴되기 전까지 유지된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Turret", meta = (ClampMin = "0.0", Units = "s"))
	float TurretLifeSpan = 0.f;

	/** 눈총과 분리된 터렛 전용 발사 설정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Turret|Weapon")
	FDRTurretWeaponSettings TurretWeaponSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Turret")
	TSubclassOf<ADRTurret> TurretClass;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Turret")
	TSubclassOf<ADRPlacementTargetActor> TargetActorClass;

	/** 포탑의 설치 영역과 유효 여부를 표현할 Preview Actor Blueprint다. */
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Turret")
	TSubclassOf<ADRPlacementPreviewActor> PreviewActorClass;

	/** 터렛 GA가 SkillDefinition로 부여되지 않을 때 사용할 쿨다운 폴백 값이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Turret|Cooldown", meta = (ClampMin = "0.0", Units = "s"))
	float TurretCooldownDuration = 0.f;

	/** 터렛 GA가 SkillDefinition로 부여되지 않을 때 사용할 쿨다운 태그 폴백이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Turret|Cooldown")
	FGameplayTag TurretCooldownTag;

private:
	void StartTargeting();
	bool ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
		FTransform& OutTurretTransform) const;
	FTransform MakeTurretTransform(const FVector& ImpactPoint, const FRotator& ViewRotation) const;
	void ResolveCooldownSettings(
		FGameplayTag& OutCooldownTag,
		float& OutCooldownDuration) const;

	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;
};
