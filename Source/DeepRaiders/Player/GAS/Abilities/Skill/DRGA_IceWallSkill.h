#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTypes.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_IceWallSkill.generated.h"

class ADRIceWall;
class ADRIceWallSegment;
class ADRPlacementPreviewActor;
class ADRPlacementTargetActor;
class UAbilityTask_WaitTargetData;

/** 바닥을 조준해 파괴 가능한 얼음벽을 설치한다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_IceWallSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_IceWallSkill();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall", meta = (ShowOnlyInnerProperties))
	FDRPlacementSettings PlacementSettings;

	/** 인디케이터와 설치 가능 범위를 나타내는 전체 얼음벽 크기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall", meta = (ClampMin = "1.0", Units = "cm"))
	FVector WallDimensions = FVector(600.f, 60.f, 300.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall|Segments", meta = (ClampMin = "1"))
	int32 SegmentCount = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall|Segments", meta = (ClampMin = "1.0"))
	float SegmentMaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall|Segments")
	TSubclassOf<ADRIceWallSegment> SegmentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall|Rise", meta = (ClampMin = "0.01", Units = "s"))
	float RiseDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Ice Wall|Lifecycle", meta = (ClampMin = "0.01", Units = "s"))
	float WallLifeSpan = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Ice Wall")
	TSubclassOf<ADRIceWall> IceWallClass;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Ice Wall")
	TSubclassOf<ADRPlacementTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Ice Wall")
	TSubclassOf<ADRPlacementPreviewActor> PreviewActorClass;

private:
	void StartTargeting();
	bool ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FTransform& OutWallTransform) const;
	FTransform MakeWallTransform(const FVector& ImpactPoint, const FRotator& ViewRotation) const;

	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;
};
