#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRShopSellPanelWidget.generated.h"

class UDRInventoryComponent;
class UDRInventoryWidget;
class UDRShopSellItemInfoWidget;
class UDRShopSellViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRShopSellPanelRequestedSignature, FGuid, InstanceId);

/** 판매 인벤토리와 선택한 아이템 정보를 표시할 패널의 기반 클래스다. */
UCLASS()
class DEEPRAIDERS_API UDRShopSellPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeInventory(UDRInventoryComponent* NewInventoryComponent);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Sell")
	FDRShopSellPanelRequestedSignature OnSellRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleEntryClicked(FGuid InstanceId);

	UFUNCTION()
	void HandleSellRequested(FGuid InstanceId);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRInventoryWidget> InventoryPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRShopSellItemInfoWidget> ItemInfoPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopSellViewModel> SellViewModel;
};
