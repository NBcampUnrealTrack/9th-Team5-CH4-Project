#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Shop/Data/DRShopTestData.h"
#include "DRShopItemWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class DEEPRAIDERS_API UDRShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItemData(const FDRShopItemData& ItemData);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void ApplyItemData();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> InformationText;

	UPROPERTY(Transient)
	FDRShopItemData ItemData;

	bool IsItemDataSet = false;
	bool IsWidgetConstructed = false;
};
