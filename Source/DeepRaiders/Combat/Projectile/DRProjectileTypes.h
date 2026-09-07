// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRProjectileTypes.generated.h"

class UCurveFloat;

// Projectile의 실제 최대 사거리를 0~1로 정규화해 위력을 감쇠시키는 설정.
USTRUCT(BlueprintType)
struct FDRProjectileFalloffSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Falloff")
	bool bEnabled = true;

	// 이 비율까지는 최대 위력을 유지한다. 이후 최대 사거리까지 0으로 감쇠한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Falloff", meta = (
		ClampMin = "0.0", ClampMax = "0.99", UIMin = "0.0", UIMax = "0.99"))
	float FullStrengthRangeRatio = 0.4f;

	// X: 정규화된 이동 거리, Y: 위력. 설정하면 선형 감쇠 대신 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Falloff")
	TObjectPtr<UCurveFloat> StrengthCurve = nullptr;

	// 위력이 0에 가까워져도 충돌체와 외형이 지나치게 작아지지 않도록 제한한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Falloff", meta = (
		ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinSizeMultiplier = 0.2f;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|World")
	EDRSnowVoxelEditTool SnowEditTool = EDRSnowVoxelEditTool::SurfaceTool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|World")
	bool bAllowVirtualSurfaceFallback = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|World")
	EDRSnowRemovalMode SnowRemovalMode = EDRSnowRemovalMode::ContactBrush;
	
};
