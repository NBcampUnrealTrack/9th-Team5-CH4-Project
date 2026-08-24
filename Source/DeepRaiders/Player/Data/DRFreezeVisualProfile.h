#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRFreezeVisualProfile.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UNiagaraSystem;


USTRUCT(BlueprintType)
struct FDRFreezeVisualPart
{
	GENERATED_BODY()

	/** 부착할 눈 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** 부착 대상 Bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze")
	FName BoneName = NAME_None;

	/** Bone 기준 기본 Transform */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze")
	FTransform AttachTransform = FTransform::Identity;

	/** 눈 파츠가 나타나기 시작하는 Freeze 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartThreshold = 0.15f;

	/** 눈 파츠가 최대 크기에 도달하는 Freeze 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndThreshold = 0.30f;

	/**
	 * AttachTransform의 Scale 기준 성장 배율.
	 *
	 * 예:
	 * BaseScale = (1,1,1)
	 * GrowthScale = (1.4,1.4,1.4)
	 * → 최종 Scale = (1.4,1.4,1.4)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze")
	FVector GrowthScale = FVector(1.4f);

	/** 성장하면서 추가로 이동시킬 Local Offset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze")
	FVector GrowthOffset = FVector::ZeroVector;
};


UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRFreezeVisualProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	// -------------------------------------------------
	// Attachments
	// -------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Attachments")
	bool bEnableAttachments = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Attachments", meta = (EditCondition = "bEnableAttachments"))
	TArray<FDRFreezeVisualPart> Parts;


	// -------------------------------------------------
	// Surface Frost
	// -------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Surface")
	bool bEnableSurfaceFrost = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Surface", meta = (EditCondition = "bEnableSurfaceFrost"))
	TObjectPtr<UMaterialInterface> FrostOverlayMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Surface", meta = ( ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableSurfaceFrost"))
	float SurfaceFrostStartThreshold = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Surface", meta = ( ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableSurfaceFrost"))
	float SurfaceFrostFullThreshold = 1.0f;


	// -------------------------------------------------
	// Niagara
	// -------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Niagara")
	bool bEnableNiagara = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Niagara", meta = (EditCondition = "bEnableNiagara"))
	TObjectPtr<UNiagaraSystem> FreezeNiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Niagara", meta = ( ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableNiagara"))
	float NiagaraStartThreshold = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Niagara", meta = ( ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableNiagara"))
	float NiagaraFullThreshold = 0.90f;


	// -------------------------------------------------
	// Smoothing
	// -------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Smoothing", meta = (ClampMin = "0.01"))
	float GrowInterpSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze|Smoothing", meta = (ClampMin = "0.01"))
	float DecayInterpSpeed = 8.f;
};
