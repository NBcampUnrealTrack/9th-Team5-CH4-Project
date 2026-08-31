#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DeepRaiders/Shop/DRShopSellTypes.h"
#include "DRShopWidget.generated.h"

class UButton;
class UDRInventoryComponent;
class UDRPerkComponent;
class UDRShopBuyPanelWidget;
class UDRShopSellPanelWidget;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopWidgetClosedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopOfferRequestedSignature,
	FDRShopOfferRequest,
	Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRShopWidgetSellRequestedSignature,
	EDRShopSellTargetType, TargetType, FGuid, InstanceId);

/** 구매·판매 패널 전환과 상점 종료를 관리하는 최상위 화면이다. */
UCLASS()
class DEEPRAIDERS_API UDRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 구매 패널에 갱신된 상품 목록을 전달한다. */
	void SetOffers(
		EDRShopOfferType OfferType,
		const TArray<FDRShopOfferView>& NewOffers);

	/** 구매·판매 패널에 로컬 플레이어 인벤토리를 연결한다. */
	void InitializeInventoryPanels(
		UDRInventoryComponent* InventoryComponent,
		UDRPerkComponent* PerkComponent);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetClosedSignature OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopOfferRequestedSignature OnOfferRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetSellRequestedSignature OnSellRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleBuyPanelButtonClicked();

	UFUNCTION()
	void HandleSellPanelButtonClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	UFUNCTION()
	void HandleSellRequested(EDRShopSellTargetType TargetType, FGuid InstanceId);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BuyPanelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SellPanelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> PanelSwitcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRShopBuyPanelWidget> BuyPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRShopSellPanelWidget> SellPanel;

};
