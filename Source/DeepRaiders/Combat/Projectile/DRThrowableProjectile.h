#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DeepRaiders/Item/DRThrowableItemTypes.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DRThrowableProjectile.generated.h"

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRThrowableProjectile : public ADRProjectile
{
	GENERATED_BODY()
	
public:
	void InitializeThrowable(UAbilitySystemComponent* InSourceAbilitySystem,
		const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
		const FDRThrowableItemSettings& InItemSettings, 
		const FDRThrowActionSettings& InActionSettings,
		int32 InSourceTeamId,
		const UObject* InPresentationSourceObject);
	
protected:
	virtual void HandleImpact(const FHitResult& ImpactResult) override;
	virtual void ExecuteImpactGameplayCue(const FHitResult& ImpactResult) override;

	virtual bool ShouldIgnoreFriendlyBlockingHit() const override
	{
		// 투척물은 기본적으로 아군과 충돌, 일단은
		return false;
	}
	
private:
	float ExplosionRadius = 300.f;
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_Visibility;
	
	FGameplayTag ImpactGameplayCueTag;
};
