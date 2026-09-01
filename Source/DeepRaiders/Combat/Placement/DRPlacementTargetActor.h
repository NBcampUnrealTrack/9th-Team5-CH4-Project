#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTypes.h"
#include "DRPlacementTargetActor.generated.h"

class ADRPlacementPreviewActor;

/** 바닥 설치형 스킬의 로컬 프리뷰 및 TargetData 전달을 담당한다. */
UCLASS(Blueprintable, NotPlaceable)
class DEEPRAIDERS_API ADRPlacementTargetActor : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	ADRPlacementTargetActor();

	void Configure(const FDRPlacementSettings& InSettings, TSubclassOf<ADRPlacementPreviewActor> InPreviewActorClass,
		const FVector& InPreviewDimensions);

	virtual void Tick(float DeltaSeconds) override;
	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual bool IsConfirmTargetingAllowed() override;
	virtual void ConfirmTargetingAndContinue() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Hit 지점이 평평한 바닥인지 검사한다. 서버 검증에서도 같은 규칙을 사용한다. */
	static bool IsValidPlacementSurface(const FHitResult& Hit, const FDRPlacementSettings& InSettings);

private:
	bool UpdateTargeting();
	void UpdatePreview(const FHitResult& Hit, const FRotator& ViewRotation, bool bCanPlace);
	void HidePreview();

	FDRPlacementSettings Settings;
	FVector PreviewDimensions = FVector::OneVector;
	FHitResult CachedAimHit;

	UPROPERTY(Transient)
	TObjectPtr<ADRPlacementPreviewActor> PreviewActor;

	TSubclassOf<ADRPlacementPreviewActor> PreviewActorClass;
	bool bHasValidAimData = false;
};
