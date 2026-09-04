#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopWeaponUpgradeWidget.generated.h"

class UVerticalBox;
class UDRWeaponUpgradeViewModel;
class UDRWeaponUpgradeEntryViewModel;
class UDRShopWeaponUpgradeEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRWeaponUpgradeRequested, FDRShopOfferRequest, Request);

UCLASS(Abstract)
class DEEPRAIDERS_API UDRShopWeaponUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeOffers(const TArray<FDRShopOfferView>& Offers);

	UFUNCTION(BlueprintCallable, Category = "Shop|MVVM")
	void SetEntries(const TArray<UDRWeaponUpgradeEntryViewModel*>& Entries);

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FDRWeaponUpgradeRequested OnOfferRequested;

private:
	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> UpgradeList;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWeaponUpgradeEntryWidget> EntryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDRWeaponUpgradeViewModel> ViewModel;
};
