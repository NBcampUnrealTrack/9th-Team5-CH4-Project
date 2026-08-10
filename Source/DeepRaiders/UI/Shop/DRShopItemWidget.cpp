#include "DRShopItemWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

void UDRShopItemWidget::SetItemDefinition(
	UDRItemDefinition* NewItemDefinition)
{
	ItemDefinition = NewItemDefinition;

	// 위젯 생성이 끝난 경우에만 바인딩된 UI에 데이터를 반영합니다.
	if (IsWidgetConstructed)
	{
		ApplyItemDefinition();
	}
}

void UDRShopItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Buy->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleBuyButtonClicked);
	IsWidgetConstructed = true;
	ApplyItemDefinition();
}

void UDRShopItemWidget::NativeDestruct()
{
	Buy->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleBuyButtonClicked);
	IsWidgetConstructed = false;
	Super::NativeDestruct();
}

void UDRShopItemWidget::ApplyItemDefinition()
{
	if (!IsValid(ItemDefinition))
	{
		return;
	}

	// 아이템 에셋의 상점 표시 정보를 갱신합니다.
	DisplayNameText->SetText(ItemDefinition->DisplayName);
	DescriptionText->SetText(ItemDefinition->Description);
	PriceText->SetText(FText::AsNumber(ItemDefinition->Price));
}

void UDRShopItemWidget::HandleBuyButtonClicked()
{
	if (IsValid(ItemDefinition))
	{
		// 실제 구매 처리는 상위 상점 위젯에 요청합니다.
		OnPurchaseRequested.Broadcast(ItemDefinition);
	}
}
