#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DeepRaiders/Item/DRItemTypes.h"
#include "DRWorldItemPresentationProfile.generated.h"

class UCurveFloat;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRWorldItemRarityVisual
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity")
	FLinearColor RarityColor = FLinearColor::White;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity", meta = (ClampMin = "0.0"))
	float Intensity = 1.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity", meta = (Clampmin = "0.0"))
	float EffectScale = 1.f;
	
	// 비어 있으면 공통 SpawnTrailSystem 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity")
	TObjectPtr<UNiagaraSystem> SpawnTrailOverride = nullptr;
	
	// 비어 있으면 공통 IdleAuraOverride 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity")
	TObjectPtr<UNiagaraSystem> IdleAuraOverride = nullptr;
};

UCLASS(BlueprintType)
class UDRWorldItemPresentationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	const FDRWorldItemRarityVisual& GetRarityVisual(EDRItemRarity Rarity) const;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Emergence")
	TObjectPtr<UNiagaraSystem> SpawnTrailSystem = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Emergence")
	TObjectPtr<UNiagaraSystem> IdleAuraSystem = nullptr;
	
	// 입력 0~1을 메시 이동 Alpha로 변환
	// 비어 있으면 EaseOut 보간 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Emergence")
	TObjectPtr<UCurveFloat> EmergenceCurve = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Emergence", meta = (ClampMin = "0.0", Units = "s"))
	float EmergenceDuration = 0.65f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Emergence", meta = (ClampMin = "0.0", Units = "cm"))
	float EmergenceArcHeight = 80.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Hover", meta = (ClampMin = "0.0", Units = "cm"))
	float HoverAmplitude = 8.f;
	
	// 초당 반복 횟수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Hover", meta = (ClampMin = "0.0", Units = "Hz"))
	float HoverFrequency = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity")
	FDRWorldItemRarityVisual DefaultRarityVisual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Item|Rarity")
	TMap<EDRItemRarity, FDRWorldItemRarityVisual> RarityVisuals;
};
