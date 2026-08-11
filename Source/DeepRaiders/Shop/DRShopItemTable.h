#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRShopItemTable.generated.h"

class UDRItemDefinition;

UENUM(BlueprintType)
enum class EDRShopOfferType : uint8
{
	Purchase,
	Upgrade
};

USTRUCT(BlueprintType)
struct FDRShopItemTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDRItemDefinition>> UpgradeDefinitions;

	bool IsUpgradeRow() const
	{
		return !UpgradeDefinitions.IsEmpty();
	}

	int32 GetMaxUpgradeLevel() const
	{
		return IsUpgradeRow() ? UpgradeDefinitions.Num() + 1 : 0;
	}

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

	bool IsValidUpgradeLevel(int32 TargetLevel) const
	{
		return TargetLevel > 0
			&& TargetLevel <= GetMaxUpgradeLevel()
			&& GetDefinitionForLevel(TargetLevel) != nullptr;
	}

	UDRItemDefinition* GetUpgradeSourceDefinition(int32 TargetLevel) const
	{
		return TargetLevel > 1 && IsValidUpgradeLevel(TargetLevel)
			? GetDefinitionForLevel(TargetLevel - 1)
			: nullptr;
	}

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

	FDRShopOfferRequest MakeRequest() const
	{
		FDRShopOfferRequest Request;
		Request.RowName = RowName;
		Request.OfferType = OfferType;
		Request.TargetLevel = TargetLevel;
		return Request;
	}
};
