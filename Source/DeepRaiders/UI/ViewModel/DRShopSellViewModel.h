#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRShopSellViewModel.generated.h"

class UDRInventoryComponent;
class UTexture2D;

/** 판매 패널에서 선택한 인벤토리 아이템의 표시 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRShopSellViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UDRInventoryComponent* InInventoryComponent);
	void Deinitialize();
	void SelectItem(FGuid InInstanceId);

	FGuid GetSelectedInstanceId() const
	{
		return SelectedInstanceId;
	}

	bool CanSell() const
	{
		return bCanSell;
	}

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop|Sell")
	TObjectPtr<UTexture2D> ItemIcon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop|Sell")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop|Sell")
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop|Sell")
	FText SellPriceText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop|Sell")
	bool bCanSell = false;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	void RefreshSelection();
	void ClearSelection();

	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	FGuid SelectedInstanceId;
};
