// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DRProjectileTypes.generated.h"

class UGameplayEffect;

// Projectile이 플레이어 적중 시 적용할 Effect 설정
USTRUCT(BlueprintType)
struct FDRProjectileImpactEffect
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect")
	TSubclassOf<UGameplayEffect> EffectClass = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect", meta = (ClampMin = 0.0, UIMin = 0.0))
	float EffectLevel = 1.0f;
	
	// Effect에 전달할 SetByCaller 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect")
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
};

// Projectile이 월드에 적중했을 때 사용할 눈 생성 설정
// 실제 눈 생성은 VoxelSystem이 담당한다.
USTRUCT(BlueprintType)
struct FDRProjectileWorldImpactData
{
	GENERATED_BODY()
public:
	// 눈생성 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|World")
	bool bAddSnow = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|World", meta = (ClampMin = 0.0, UIMin = 0.0))
	float SnowRadius = 50.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|World", meta = (ClampMin = 0.0, UIMin = 0.0))
	float SnowAmount = 1.0f;
	
};
