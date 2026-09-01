#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DeepRaiders/Item/DRThrowableItemTypes.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DRThrowableProjectile.generated.h"

class UDRSnowAddComponent;
class UDRSnowRemoveComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRThrowableProjectile : public ADRProjectile
{
	GENERATED_BODY()
	
public:
	ADRThrowableProjectile(const FObjectInitializer& ObjectInitializer);
	
	void InitializeThrowable(UAbilitySystemComponent* InSourceAbilitySystem,
		const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
		const FDRThrowableItemSettings& InItemSettings, 
		const FDRThrowActionSettings& InActionSettings,
		int32 InSourceTeamId,
		const UObject* InPresentationSourceObject);
	
protected:
	virtual void HandleImpact(const FHitResult& ImpactResult) override;
	virtual void HandleWorldImpact(const FHitResult& ImpactResult) override;
	virtual void ExecuteImpactGameplayCue(const FHitResult& ImpactResult) override;

	virtual bool ShouldIgnoreFriendlyBlockingHit() const override
	{
		// 투척물은 기본적으로 아군과 충돌, 일단은
		return false;
	}
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Snow", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDRSnowAddComponent> SnowAddComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Snow", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDRSnowRemoveComponent> SnowRemoveComponent;
	
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_Visibility;
	
	FGameplayTag ImpactGameplayCueTag;
	FDRThrowableItemSettings ItemSettings;
};
