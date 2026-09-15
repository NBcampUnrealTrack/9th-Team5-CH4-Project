#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopItemWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UDRShopOfferEntryViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopItemOfferRequestedSignature,
	FDRShopOfferRequest,
	Request);

UCLASS()
class DEEPRAIDERS_API UDRShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeViewModel(UDRShopOfferEntryViewModel* NewViewModel);

	/** 위젯에 표시할 상점 Offer 데이터를 설정한다. */
	void SetOffer(const FDRShopOfferView& NewOffer);

	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopItemOfferRequestedSignature OnOfferRequested;

protected:
	/** 바인딩된 위젯을 확인하고 구매 버튼 이벤트를 연결한다. */
	virtual void NativeConstruct() override;

	/** 구매 버튼 이벤트 연결을 해제한다. */
	virtual void NativeDestruct() override;

private:
	/** 현재 Offer의 이름, 설명, 가격, 아이콘과 구매 가능 상태를 표시한다. */
	void ApplyOffer();

	/** 유효한 Offer의 구매 요청을 상위 상점 위젯에 전달한다. */
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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(Transient)
	FDRShopOfferView Offer;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|MVVM")
	FName EntryViewModelName = TEXT("ShopOfferEntryViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRShopOfferEntryViewModel> EntryViewModel;

	bool IsWidgetConstructed = false;
};
