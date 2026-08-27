#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRShopItemTable.generated.h"

class UDRItemDefinition;
class UTexture2D;

UENUM(BlueprintType)
enum class EDRShopOfferType : uint8
{
	Purchase,
	Upgrade,
	Perk
};

UENUM(BlueprintType)
enum class EDRShopOfferSection : uint8
{
	Equipment,
	Consumable,
	Upgrade,
	Perk
};

USTRUCT(BlueprintType)
struct FDRShopItemTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDRItemDefinition>> UpgradeDefinitions;

	/** 단계별 Definition이 설정된 업그레이드 Row인지 확인한다. */
	bool IsUpgradeRow() const
	{
		return !UpgradeDefinitions.IsEmpty();
	}

	/** 기본 아이템을 포함한 전체 업그레이드 단계 수를 반환한다. */
	int32 GetMaxUpgradeLevel() const
	{
		return IsUpgradeRow() ? UpgradeDefinitions.Num() + 1 : 0;
	}

	/** 지정한 단계의 아이템 Definition을 반환한다. */
	UDRItemDefinition* GetDefinitionForLevel(int32 Level) const
	{
		if (Level <= 0 || Level > UpgradeDefinitions.Num() + 1)
		{
			return nullptr;
		}

		return Level == 1
			? ItemDefinition.Get()
			: UpgradeDefinitions[Level - 2].Get();
	}

	/** 요청 단계가 업그레이드 체인 범위 안에 있는지 확인한다. */
	bool IsValidUpgradeLevel(int32 TargetLevel) const
	{
		return TargetLevel > 0
			&& TargetLevel <= GetMaxUpgradeLevel()
			&& GetDefinitionForLevel(TargetLevel) != nullptr;
	}

	/** 목표 단계 바로 이전의 Definition을 반환한다. */
	UDRItemDefinition* GetUpgradeSourceDefinition(int32 TargetLevel) const
	{
		return TargetLevel > 1 && IsValidUpgradeLevel(TargetLevel)
			? GetDefinitionForLevel(TargetLevel - 1)
			: nullptr;
	}

	/** 목표 단계에서 지급할 Definition을 반환한다. */
	UDRItemDefinition* GetUpgradeTargetDefinition(int32 TargetLevel) const
	{
		return IsValidUpgradeLevel(TargetLevel)
			? GetDefinitionForLevel(TargetLevel)
			: nullptr;
	}
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
	int32 TargetLevel = 0;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDRItemDefinition> UpgradeSourceDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 TargetLevel = 0;

	bool IsUpgrade() const
	{
		return OfferType == EDRShopOfferType::Upgrade;
	}

	/** UI Offer를 서버에 전달할 최소 요청 데이터로 변환한다. */
	FDRShopOfferRequest MakeRequest() const
	{
		FDRShopOfferRequest Request;
		Request.RowName = RowName;
		Request.OfferType = OfferType;
		Request.TargetLevel = TargetLevel;
		return Request;
	}
};
