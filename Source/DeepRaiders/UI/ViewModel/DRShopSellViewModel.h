#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Shop/DRShopSellTypes.h"
#include "MVVMViewModelBase.h"
#include "DRShopSellViewModel.generated.h"

class UDRInventoryComponent;
class UDRPerkComponent;
class UTexture2D;

/** 판매 패널에서 선택한 인벤토리 아이템의 표시 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRShopSellViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UDRInventoryComponent* InInventoryComponent, UDRPerkComponent* InPerkComponent);
	void Deinitialize();
	void SelectItem(FGuid InInstanceId);
	void SelectPerk(FGuid InPerkInstanceId);

	FGuid GetSelectedInstanceId() const
	{
		return SelectedInstanceId;
	}

	bool CanSell() const
	{
		return bCanSell;
	}

	EDRShopSellTargetType GetSelectedTargetType() const
	{
		return SelectedTargetType;
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

	UFUNCTION()
	void HandlePerksChanged();

	void RefreshSelection();
	void ClearSelection();

	TWeakObjectPtr<UDRInventoryComponent> InventoryComponent;
	TWeakObjectPtr<UDRPerkComponent> PerkComponent;
	FGuid SelectedInstanceId;
	EDRShopSellTargetType SelectedTargetType = EDRShopSellTargetType::Item;
};
