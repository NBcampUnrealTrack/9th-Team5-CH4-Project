#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRShopWeaponUpgradeWidget.h"
#include "DRShopWeaponUpgradeEntryWidget.generated.h"

class UButton;
class UDRWeaponUpgradeEntryViewModel;

UCLASS(Abstract)
class DEEPRAIDERS_API UDRShopWeaponUpgradeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeViewModel(UDRWeaponUpgradeEntryViewModel* NewViewModel);

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FDRWeaponUpgradeRequested OnOfferRequested;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleUpgradeClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UDRWeaponUpgradeEntryViewModel> ViewModel;
};
