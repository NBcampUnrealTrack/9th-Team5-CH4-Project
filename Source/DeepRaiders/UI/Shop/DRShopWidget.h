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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopSellAllOresRequestedSignature);

UCLASS()
class DEEPRAIDERS_API UDRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 상점 상품 목록을 저장하고 기본 카테고리를 표시한다. */
	void InitializeShop(
		const TArray<TObjectPtr<UDRItemDefinition>>& ItemDefinitions);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetClosedSignature OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopPurchaseRequestedSignature OnPurchaseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopSellAllOresRequestedSignature OnSellAllOresRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	/** WBP에 버튼이 없으면 카테고리 영역에 전체 판매 버튼을 생성한다. */
	void InitializeSellAllOresButton();

	/** 선택된 카테고리 버튼 상태와 상품 목록을 갱신한다. */
	void SelectCategory(EItemCategory Category);

	/** 선택된 카테고리의 상품 위젯을 다시 생성한다. */
	void RefreshItems(EItemCategory Category);

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void HandleEquipmentButtonClicked();

	UFUNCTION()
	void HandleConsumableButtonClicked();

	/** 전체 판매 요청을 상점 컴포넌트로 전달한다. */
	UFUNCTION()
	void HandleSellAllOresButtonClicked();

	/** 상품 위젯의 구매 요청을 상점 컴포넌트로 전달한다. */
	UFUNCTION()
	void HandlePurchaseRequested(UDRItemDefinition* ItemDefinition);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SellAllOresButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemScrollBox;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopItemWidget> ItemWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRItemDefinition>> ItemDefinitions;
};
