#include "DRShopWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Shop/DRShopTestPlayerState.h"
#include "DRShopItemWidget.h"

void UDRShopWidget::InitializeShop(
	const TArray<TObjectPtr<UDRItemDefinition>>& NewItemDefinitions,
	ADRShopTestPlayerState* NewPlayerState)
{
	// 상점 목록과 코인 정보를 초기화합니다.
	ItemDefinitions = NewItemDefinitions;
	SetPlayerState(NewPlayerState);
	SelectCategory(EItemCategory::Equipment);
}

void UDRShopWidget::SelectCategory(EItemCategory Category)
{
	// 비활성화 버튼 스타일로 현재 선택된 탭을 표시합니다.
	EquipmentButton->SetIsEnabled(Category != EItemCategory::Equipment);
	ConsumableButton->SetIsEnabled(Category != EItemCategory::Consumable);
	RefreshItems(Category);
}

void UDRShopWidget::RefreshItems(EItemCategory Category)
{
	if (!ItemWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Shop items initialization failed: ItemWidgetClass is invalid."));
		return;
	}

	if (!IsValid(ItemScrollBox))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Shop items initialization failed: ItemScrollBox is invalid."));
		return;
	}

	// 선택한 카테고리에 해당하는 아이템 위젯만 다시 생성합니다.
	ItemScrollBox->ClearChildren();
	int32 CreatedItemCount = 0;

	for (UDRItemDefinition* ItemDefinition : ItemDefinitions)
	{
		if (!IsValid(ItemDefinition)
			|| ItemDefinition->Category != Category)
		{
			continue;
		}

		UDRShopItemWidget* ItemWidget =
			CreateWidget<UDRShopItemWidget>(
				GetOwningPlayer(),
				ItemWidgetClass);

		if (!IsValid(ItemWidget))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Shop item widget creation failed: Item=%s."),
				*ItemDefinition->DisplayName.ToString());
			continue;
		}

		ItemWidget->SetItemDefinition(ItemDefinition);
		ItemWidget->OnPurchaseRequested.AddDynamic(
			this,
			&ThisClass::HandlePurchaseRequested);
		ItemScrollBox->AddChild(ItemWidget);
		++CreatedItemCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("Shop category items created successfully: %d items."),
		CreatedItemCount);
}

void UDRShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 위젯 수명 동안 한 번만 버튼 이벤트를 연결합니다.
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}

	EquipmentButton->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleEquipmentButtonClicked);
	ConsumableButton->OnClicked.AddDynamic(
		this,
		&ThisClass::HandleConsumableButtonClicked);
}

void UDRShopWidget::NativeDestruct()
{
	SetPlayerState(nullptr);

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleCloseButtonClicked);
	}

	EquipmentButton->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleEquipmentButtonClicked);
	ConsumableButton->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleConsumableButtonClicked);

	Super::NativeDestruct();
}

void UDRShopWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UDRShopWidget::HandleEquipmentButtonClicked()
{
	SelectCategory(EItemCategory::Equipment);
}

void UDRShopWidget::HandleConsumableButtonClicked()
{
	SelectCategory(EItemCategory::Consumable);
}

void UDRShopWidget::SetPlayerState(ADRShopTestPlayerState* NewPlayerState)
{
	// 코인 변경 이벤트가 중복 등록되지 않도록 관리합니다.
	if (IsValid(PlayerState))
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCoinsChanged);
	}

	PlayerState = NewPlayerState;

	if (IsValid(PlayerState))
	{
		PlayerState->OnCoinsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleCoinsChanged);
		HandleCoinsChanged(PlayerState->GetCoins());
	}
}

void UDRShopWidget::HandleCoinsChanged(int32 NewCoins)
{
	if (IsValid(CoinsText))
	{
		CoinsText->SetText(FText::AsNumber(NewCoins));
	}
}

void UDRShopWidget::HandlePurchaseRequested(
	UDRItemDefinition* ItemDefinition)
{
	OnPurchaseRequested.Broadcast(ItemDefinition);
}
