#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Styling/SlateBrush.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRWeaponUpgradeViewModel.generated.h"

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRWeaponUpgradeEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(const FDRShopOfferView& NewOffer);
	bool GetPurchaseRequest(FDRShopOfferRequest& Request) const;

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FText LevelText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FText LevelStepsText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FText UpgradeButtonText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	bool IsPurchasable = false;

private:
	UPROPERTY(Transient)
	FDRShopOfferView Offer;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRWeaponUpgradeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<FDRShopOfferView>& Offers);

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FText WeaponName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	FSlateBrush WeaponBrush;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Upgrade")
	TArray<TObjectPtr<UDRWeaponUpgradeEntryViewModel>> Entries;
};
