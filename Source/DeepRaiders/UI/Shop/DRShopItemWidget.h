#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRShopItemWidget.generated.h"

class UDRItemDefinition;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopItemPurchaseRequestedSignature,
	UDRItemDefinition*,
	ItemDefinition);

UCLASS()
class DEEPRAIDERS_API UDRShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItemDefinition(UDRItemDefinition* NewItemDefinition);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopItemPurchaseRequestedSignature OnPurchaseRequested;

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
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	bool IsWidgetConstructed = false;
};
