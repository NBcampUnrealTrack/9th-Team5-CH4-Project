#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "DRShopItemTable.generated.h"

class UDRItemDefinition;
class UTexture2D;

UENUM(BlueprintType)
enum class EDRShopOfferType : uint8
{
	Purchase,
	Perk = 2,
	CharacterUpgrade = 3
};

UENUM(BlueprintType)
enum class EDRShopOfferSection : uint8
{
	Equipment,
	Consumable,
	Perk = 3,
	CharacterUpgrade = 4
};

USTRUCT(BlueprintType)
struct FDRShopItemTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDRItemDefinition> ItemDefinition;
};

USTRUCT(BlueprintType)
struct FDRShopOfferRequest
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	FName RowName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	EDRShopOfferType OfferType = EDRShopOfferType::Purchase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	FGameplayTag UpgradeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 ExpectedLevel = 0;

	bool IsValidRequest() const
	{
		switch (OfferType)
		{
		case EDRShopOfferType::CharacterUpgrade:
			return UpgradeTag.IsValid() && ExpectedLevel >= 0 && ExpectedLevel < MAX_int32;
		case EDRShopOfferType::Purchase:
		case EDRShopOfferType::Perk:
			return !RowName.IsNone();
		default:
			return false;
		}
	}
};

USTRUCT(BlueprintType)
struct FDRShopOfferView
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	FDRShopOfferRequest Request;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	EDRShopOfferSection Section = EDRShopOfferSection::Equipment;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	FText Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 Price = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	bool IsPurchasable = true;
};

USTRUCT(BlueprintType)
struct FDRShopItemOffer
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	FName RowName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	EDRShopOfferType OfferType = EDRShopOfferType::Purchase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	/** UI Offer를 서버에 전달할 최소 요청 데이터로 변환한다. */
	FDRShopOfferRequest MakeRequest() const
	{
		FDRShopOfferRequest Request;
		Request.RowName = RowName;
		Request.OfferType = OfferType;
		return Request;
	}
};
