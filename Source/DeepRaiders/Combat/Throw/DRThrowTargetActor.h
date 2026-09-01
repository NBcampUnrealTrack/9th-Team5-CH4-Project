#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "DeepRaiders/Item/DRThrowableItemTypes.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DRThrowTargetActor.generated.h"

class UNiagaraComponent;

UCLASS(Blueprintable, NotPlaceable)
class DEEPRAIDERS_API ADRThrowTargetActor : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()
	
public:
	ADRThrowTargetActor();
	
	void Configure(const FDRThrowableItemSettings& InItemSettings, const FDRThrowActionSettings& InActionSettings,
		bool bInShowTrajectory);

	virtual void Tick(float DeltaSeconds) override;
	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual bool IsConfirmTargetingAllowed() override;
	virtual void ConfirmTargetingAndContinue() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	bool UpdateTargeting();
	void UpdateTrajectoryVFX(const TArray<FVector>& PathPoints, const TArray<FVector>& PathDirections);
	void DestroyTrajectoryVFX();
	
	FDRThrowableItemSettings ItemSettings;
	FDRThrowActionSettings ActionSettings;
	FHitResult CachedAimHit;
	
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrajectoryComponent;
	
	bool bShowTrajectory = false;
	bool bHasValidAimData = false;
};
