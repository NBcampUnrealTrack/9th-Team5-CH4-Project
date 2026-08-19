#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopWidget.generated.h"

class UButton;
class UDRShopItemWidget;
class UScrollBox;
enum class EDRItemCategory : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopWidgetClosedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopOfferRequestedSignature,
	FDRShopOfferRequest,
	Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopSellAllOresRequestedSignature);

UCLASS()
class DEEPRAIDERS_API UDRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeShop(const TArray<FDRShopOfferView>& NewItemOffers);
	void SetUpgradeOffers(const TArray<FDRShopOfferView>& NewUpgradeOffers);
	void SetPerkOffers(const TArray<FDRShopOfferView>& NewPerkOffers);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetClosedSignature OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopOfferRequestedSignature OnOfferRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopSellAllOresRequestedSignature OnSellAllOresRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void InitializeSellAllOresButton();
	void InitializeUpgradeButton();
	void InitializePerkButton();
	void SelectCategory(EDRItemCategory Category);
	void RefreshItems(EDRItemCategory Category);
	void RefreshUpgradeItems();
	void RefreshPerkItems();
	bool CreateItemWidget(const FDRShopOfferView& Offer);

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void HandleEquipmentButtonClicked();

	UFUNCTION()
	void HandleConsumableButtonClicked();

	UFUNCTION()
	void HandleUpgradeButtonClicked();

	UFUNCTION()
	void HandlePerkButtonClicked();

	UFUNCTION()
	void HandleSellAllOresButtonClicked();

	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SellAllOresButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PerkButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemScrollBox;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopItemWidget> ItemWidgetClass;

	UPROPERTY(Transient)
	TArray<FDRShopOfferView> ItemOffers;

	UPROPERTY(Transient)
	TArray<FDRShopOfferView> UpgradeOffers;

	UPROPERTY(Transient)
	TArray<FDRShopOfferView> PerkOffers;

	bool IsUpgradeSelected = false;
	bool IsPerkSelected = false;
};
