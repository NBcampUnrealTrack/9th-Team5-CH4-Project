#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopItemWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopItemOfferRequestedSignature,
	FDRShopOfferRequest,
	Request);

UCLASS()
class DEEPRAIDERS_API UDRShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItemOffer(const FDRShopItemOffer& NewItemOffer);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopItemOfferRequestedSignature OnOfferRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void ApplyItemDefinition();

	UFUNCTION()
	void HandleBuyButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Buy;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DisplayNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(Transient)
	FDRShopItemOffer ItemOffer;

	bool IsWidgetConstructed = false;
};
