#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopWidget.generated.h"

class UButton;
class UDRShopItemWidget;
class UScrollBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopWidgetClosedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopOfferRequestedSignature,
	FDRShopOfferRequest,
	Request);

UCLASS()
class DEEPRAIDERS_API UDRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOffers(
		EDRShopOfferType OfferType,
		const TArray<FDRShopOfferView>& NewOffers);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetClosedSignature OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopOfferRequestedSignature OnOfferRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void SelectSection(EDRShopOfferSection Section);
	void RefreshSelectedSection();
	void CreateItemWidget(const FDRShopOfferView& Offer);

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
	void HandleOfferRequested(FDRShopOfferRequest Request);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PerkButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemScrollBox;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopItemWidget> ItemWidgetClass;

	UPROPERTY(Transient)
	TArray<FDRShopOfferView> Offers;

	EDRShopOfferSection SelectedSection = EDRShopOfferSection::Equipment;
};
