#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTypes.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_SnowWallSkill.generated.h"

class ADRPlacementTargetActor;
class ADRPlacementPreviewActor;
class ADRSnowWall;
class AVoxelWorld;
class UAbilityTask_WaitTargetData;
class UWorld;

/** 바닥을 조준해 눈 벽을 설치하는 스킬이다. 프리뷰는 로컬, 실제 벽 생성은 서버만 담당한다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_SnowWallSkill : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UDRGA_SnowWallSkill();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Snow Wall", meta = (ShowOnlyInnerProperties))
	FDRPlacementSettings PlacementSettings;

	/** 눈 벽만의 실제 충돌 및 시각적 크기다. 프리뷰는 이 값을 그대로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Snow Wall", meta = (ClampMin = "1.0", Units = "cm"))
	FVector WallDimensions = FVector(600.f, 60.f, 300.f);

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Snow Wall")
	TSubclassOf<ADRSnowWall> SnowWallClass;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Snow Wall")
	TSubclassOf<ADRPlacementTargetActor> TargetActorClass;

	/** 눈 벽의 모양과 유효/무효 색상은 이 Preview Actor Blueprint에서 정의한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Snow Wall")
	TSubclassOf<ADRPlacementPreviewActor> PreviewActorClass;

private:
	void StartTargeting();
	bool ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FTransform& OutWallTransform,
		FHitResult& OutSurfaceHit) const;
	FTransform MakeWallTransform(const FVector& ImpactPoint, const FRotator& ViewRotation) const;
	void LiftActorsOntoWall(UWorld* World, const FTransform& WallTransform, const FHitResult& SurfaceHit) const;
	AVoxelWorld* ResolveVoxelWorld(const FHitResult& SurfaceHit) const;

	UFUNCTION()
	void HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;
};
