#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRShopWidget.generated.h"

class UButton;
class UDRItemDefinition;
class UDRShopItemWidget;
class UScrollBox;
enum class EItemCategory : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopWidgetClosedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopPurchaseRequestedSignature,
	UDRItemDefinition*,
	ItemDefinition);

UCLASS()
class DEEPRAIDERS_API UDRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeShop(
		const TArray<TObjectPtr<UDRItemDefinition>>& ItemDefinitions);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetClosedSignature OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopPurchaseRequestedSignature OnPurchaseRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void SelectCategory(EItemCategory Category);
	void RefreshItems(EItemCategory Category);

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void HandleEquipmentButtonClicked();

	UFUNCTION()
	void HandleConsumableButtonClicked();

	UFUNCTION()
	void HandlePurchaseRequested(UDRItemDefinition* ItemDefinition);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemScrollBox;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopItemWidget> ItemWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRItemDefinition>> ItemDefinitions;
};
