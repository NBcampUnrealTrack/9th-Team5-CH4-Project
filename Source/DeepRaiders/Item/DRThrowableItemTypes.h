#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DRThrowableItemTypes.generated.h"

/*
 * 투척 아이템 정적 설정
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRThrowableItemSettings
{
	GENERATED_BODY()
	
	// 투사체가 충돌했을 때 폭발 범위 내 대상에게 적용할 GameplayEffect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Effect")
	TArray<FDRGameplayEffectData> ImpactEffects;
	
	// 투척 직후 초기 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Movement")
	float InitialSpeed = 1400.f;
	
	// 중력 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Movement")
	float GravityScale = 1.f;
	
	// 충돌 지점을 중심으로 GameplayEffect를 적용할 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Effect")
	float ExplosionRadius = 300.f;
	
	// 최대 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Aim")
	float MaxAimDistance = 3000.f;
};