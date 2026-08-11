// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRUsableDiggingDefinition.generated.h"

UCLASS(BlueprintType, AutoExpandCategories = ("Digging", "Digging|Timing"))
class DEEPRAIDERS_API UDRUsableDiggingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging", meta = (ClampMin = "0.0", Units = "cm"))
	float DigRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float StartDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging|Timing")
	uint8 bDestroyOwnerOnFinished : 1 = true;
};
