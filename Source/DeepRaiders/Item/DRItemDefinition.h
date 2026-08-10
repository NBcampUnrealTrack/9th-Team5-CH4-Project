// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRItemDefinition.generated.h"

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Ore,
	Equipment,
	Consumable,
	End,
};

/**
 * 
 */
UCLASS(BlueprintType, AutoExpandCategories = ( "Item", "Item|Trade", "Item|Mesh"))
class DEEPRAIDERS_API UDRItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemCategory Category;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText Description;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1, UIMin = 1))
	int32 MaxStackSize = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Trade")
	uint8 bCanBeSold:1 = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Trade", meta = (ClampMin = 1, UIMin = 1))
	int32 Price = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh")
	TObjectPtr<UStaticMesh> WorldMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh", meta=(ShowOnlyInnerProperties))
	FTransform SpawnOffsetTransform;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh", meta=(ShowOnlyInnerProperties))
	FTransform FirstPersonVisualOffsetTransform;
};

