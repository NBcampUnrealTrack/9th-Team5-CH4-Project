#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRFreezeVisualProfile.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct FDRFreezeVisualPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	FTransform AttachTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StartThreshold = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze", meta=(ClampMin="0.0", ClampMax="1.0"))
	float EndThreshold = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	FVector GrowthScale = FVector(1.4f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	FVector GrowthOffset = FVector::ZeroVector;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRFreezeVisualProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze|Parts")
	TArray<FDRFreezeVisualPart> Parts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze|Smoothing", meta=(ClampMin="0.01"))
	float GrowInterpSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze|Smoothing", meta=(ClampMin="0.01"))
	float DecayInterpSpeed = 8.f;
};
