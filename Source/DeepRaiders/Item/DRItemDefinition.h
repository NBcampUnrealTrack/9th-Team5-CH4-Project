// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DRItemTypes.h"
#include "DRItemDefinition.generated.h"

class ADRWorldItemActor;
class UDRAbilitySet;
class UTexture2D;
class USoundBase;
class UDRItemAnimationSet;
class UDRWorldItemPresentationProfile;

UENUM(BlueprintType)
enum class EDRItemAbilityLifetimePolicy : uint8
{
	EquippedOnly UMETA(DisplayName = "Equipped Only"),
	KeepWhileActive UMETA(DisplayName = "Keep While Active"),
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRItemDataTableRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RowName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDRItemCategory Category;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDRItemRarity Rarity = EDRItemRarity::Common;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxStackSize = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bCanBeSold:1 = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Price = 0;	
};

UCLASS(BlueprintType, AutoExpandCategories = ( "Item", "Item|Trade", "Item|Mesh"))
class DEEPRAIDERS_API UDRItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	bool IsSellable() const
	{
		return bCanBeSold && Price >= 2;
	}

	int32 GetSellPrice() const
	{
		return IsSellable() ? Price / 2 : 0;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EDRItemCategory Category;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EDRItemRarity Rarity = EDRItemRarity::Common;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Interaction")
	FText WorldInteractionText = NSLOCTEXT("DRInteraction", "DefaultItemInteractionAction", "상호작용");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|UI")
	TObjectPtr<UTexture2D> Icon;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText Description;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1, UIMin = 1))
	int32 MaxStackSize = 1;
	
	// 아이템 버리기, 사망 시 드랍 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Policy")
	uint8 bCanBeDropped : 1 = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Trade")
	uint8 bCanBeSold:1 = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Trade", meta = (ClampMin = 0, UIMin = 0))
	int32 Price = 0;

	// ===== GAS =====
	
	// 아이템이 장착되었을 때 ASC에 부여할 Ability와 Effect 셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|GAS")
	TObjectPtr<UDRAbilitySet> ItemAbilitySet;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|GAS")
	EDRItemAbilityLifetimePolicy AbilityLifetimePolicy = EDRItemAbilityLifetimePolicy::EquippedOnly;
	
	// Mesh
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh")
	TObjectPtr<UStaticMesh> WorldMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Mesh", meta=(ShowOnlyInnerProperties))
	FTransform WorldItemOffsetTransform;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSubclassOf<ADRWorldItemActor> ActorClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|World Presentation")
	TObjectPtr<UDRWorldItemPresentationProfile> WorldItemPresentationProfile = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Presentation")
	FName HandAttachSocketName = TEXT("S_HandGrip_R");
	
	// Sound
	
	/** 아이템이 활성화될 때 재생할 소리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound")
	TObjectPtr<USoundBase> ActiveSound;

	/** 아이템을 주웠을 때 재생할 소리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound")
	TObjectPtr<USoundBase> PickupSound;

	/** 아이템이 땅에 떨어졌을 때 재생할 소리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound")
	TObjectPtr<USoundBase> DroppedSound;
	
	// ===== Animation =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Animation")
	TObjectPtr<UDRItemAnimationSet> ItemAnimationSet;
};

