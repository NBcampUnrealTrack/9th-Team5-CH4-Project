#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRThrowableItemTypes.h"
#include "DRWeaponPresentationTypes.h"
#include "DRThrowableItemDefinition.generated.h"

class ADRThrowableProjectile;

/* 
 * 투척하여 충돌 지점 주변 대상에게 효과를 적용하는 소모성 아이템 Definition
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRThrowableItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()
	
public:
	UDRThrowableItemDefinition();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable", meta = (ShowOnlyInnerProperties))
	FDRThrowableItemSettings ThrowSettings;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable")
	TSubclassOf<ADRThrowableProjectile> ProjectileClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation")
	FDRWeaponPresentationData ThrowPresentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation")
	FDRWeaponPresentationData ImpactPresentation;
	
	// 투척물이 손을 떠나는 시점에 실행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Presentation",
		meta = (GameplayTagFilter = "GameplayCue"))
	FGameplayTag ThrowGameplayCueTag;
	
	// 충돌 시 실행할 일회성 GameplayCue
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Presentation",
		meta = (GameplayTagFilter = "GameplayCue"))
	FGameplayTag ImpactGameplayCueTag;
};
