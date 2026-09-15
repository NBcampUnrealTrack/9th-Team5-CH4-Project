#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/DRShopSellTypes.h"
#include "DRShopSellItemInfoWidget.generated.h"

class UButton;
class UDRShopSellViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRShopSellRequestedSignature,
	EDRShopSellTargetType, TargetType, FGuid, InstanceId);

/** 선택한 판매 아이템 정보를 표시하고 판매 요청을 전달한다. */
UCLASS()
class DEEPRAIDERS_API UDRShopSellItemInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeViewModel(UDRShopSellViewModel* NewViewModel);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Sell")
	FDRShopSellRequestedSignature OnSellRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleSellButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SellButton;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|MVVM")
	FName SellViewModelName = TEXT("DRShopSellViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRShopSellViewModel> SellViewModel;
};
