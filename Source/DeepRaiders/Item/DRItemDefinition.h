// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRItemActionTypes.h"
#include "DRItemDefinition.generated.h"

class ADRWorldItemActor;
class USoundBase;

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

	// ===== Action =====

	/** 좌클릭으로 실행할 기본 행동 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Action")
	EDRItemActionType PrimaryAction =
		EDRItemActionType::None;

	/** 우클릭으로 실행할 보조 행동 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Action")
	EDRItemActionType SecondaryAction =
		EDRItemActionType::None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh")
	TObjectPtr<UStaticMesh> WorldMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh", meta=(ShowOnlyInnerProperties))
	FTransform SpawnOffsetTransform;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh", meta=(ShowOnlyInnerProperties))
	FTransform FirstPersonVisualOffsetTransform;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSubclassOf<ADRWorldItemActor> ActorClass;

	/** 아이템이 채굴되어 드러날 때 재생할 소리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound")
	TObjectPtr<USoundBase> MinedSound;

	/** 아이템을 주웠을 때 재생할 소리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound")
	TObjectPtr<USoundBase> PickupSound;

	/** 아이템이 땅에 떨어졌을 때 재생할 소리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound")
	TObjectPtr<USoundBase> DroppedSound;
};

