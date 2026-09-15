#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopBuyPanelWidget.generated.h"

class UButton;
class UDRInventoryComponent;
class UDRInventoryWidget;
class UDRShopItemWidget;
class UDRShopWeaponUpgradeWidget;
class UDRShopOfferEntryViewModel;
class UDRShopViewModel;
class UScrollBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopBuyPanelOfferRequestedSignature,
	FDRShopOfferRequest,
	Request);

/** 구매 탭과 상품 목록을 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRShopBuyPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 구매 패널에 로컬 플레이어 인벤토리를 연결한다. */
	void InitializeInventory(UDRInventoryComponent* InventoryComponent);

	void SetOffers(
		EDRShopOfferType OfferType,
		const TArray<FDRShopOfferView>& NewOffers);

	/** ViewModel 상품 목록을 실제 Entry 위젯으로 변환한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|MVVM")
	void SetOfferEntries(const TArray<UDRShopOfferEntryViewModel*>& NewOfferEntries);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopBuyPanelOfferRequestedSignature OnOfferRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void SelectSection(EDRShopOfferSection Section);
	void SetWeaponUpgradeEntries(const TArray<UDRShopOfferEntryViewModel*>& NewOfferEntries);

	UFUNCTION()
	void HandleEquipmentButtonClicked();

	UFUNCTION()
	void HandleConsumableButtonClicked();

	UFUNCTION()
	void HandlePerkButtonClicked();

	UFUNCTION()
	void HandleCharacterUpgradeButtonClicked();

	UFUNCTION()
	void HandleWeaponUpgradeButtonClicked();

	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PerkButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CharacterUpgradeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> WeaponUpgradeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRInventoryWidget> PlayerInventory;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopItemWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWeaponUpgradeWidget> WeaponUpgradeWidgetClass;

	/** Widget Blueprint에 등록한 Manual ViewModel 이름이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Shop|MVVM")
	FName ShopViewModelName = TEXT("DRShopViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRShopViewModel> ShopViewModel;

	EDRShopOfferSection SelectedSection = EDRShopOfferSection::Equipment;
};
