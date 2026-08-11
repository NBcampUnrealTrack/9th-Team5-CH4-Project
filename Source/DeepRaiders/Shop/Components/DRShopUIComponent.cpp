#include "DRShopUIComponent.h"

#include "DRInteractionComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRShopUIComponent::LoadItemDefinitions()
{
	ItemDefinitions.Reset();

	if (!IsValid(ItemTable))
	{
		return;
	}

	TArray<FDRShopItemTableRow*> ItemRows;
	ItemTable->GetAllRows(TEXT("LoadItemDefinitions"), ItemRows);

	for (const FDRShopItemTableRow* ItemRow : ItemRows)
	{
		if (ItemRow && IsValid(ItemRow->ItemDefinition))
		{
			ItemDefinitions.AddUnique(ItemRow->ItemDefinition);
		}
	}
}

bool UDRShopUIComponent::IsItemAvailable(
	const UDRItemDefinition* ItemDefinition) const
{
	return IsValid(ItemDefinition)
		&& ItemDefinitions.Contains(ItemDefinition);
}

bool UDRShopUIComponent::CanPurchase(
	const APawn* Interactor,
	const UDRItemDefinition* ItemDefinition) const
{
	// 상점 상품 여부와 플레이어의 상호작용 범위를 함께 검증합니다.
	return IsItemAvailable(ItemDefinition)
		&& IsValid(InteractionComponent)
		&& IsValid(Interactor)
		&& InteractionComponent->IsOverlappingActor(Interactor);
}

bool UDRShopUIComponent::IsSellAllowed(const APawn* Interactor) const
{
	return IsValid(InteractionComponent)
		&& IsValid(Interactor)
		&& InteractionComponent->IsOverlappingActor(Interactor);
}

void UDRShopUIComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadItemDefinitions();

	InteractionComponent =
		GetOwner()->FindComponentByClass<UDRInteractionComponent>();

	if (!IsValid(InteractionComponent))
	{
		return;
	}

	// 상호작용 범위 진입 및 이탈 이벤트를 구독합니다.
	InteractionComponent->OnInteractionEntered.AddDynamic(
		this,
		&ThisClass::HandleInteractionEntered);
	InteractionComponent->OnInteractionExited.AddDynamic(
		this,
		&ThisClass::HandleInteractionExited);
}

void UDRShopUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(InteractionComponent))
	{
		// EndPlay 이후 이벤트가 호출되지 않도록 구독을 해제합니다.
		InteractionComponent->OnInteractionEntered.RemoveDynamic(
			this,
			&ThisClass::HandleInteractionEntered);
		InteractionComponent->OnInteractionExited.RemoveDynamic(
			this,
			&ThisClass::HandleInteractionExited);
	}

	HideShopWidget();
	Super::EndPlay(EndPlayReason);
}

void UDRShopUIComponent::HandleInteractionEntered(APawn* Interactor)
{
	// 로컬 플레이어의 중복 UI 생성을 방지합니다.
	if (!IsValid(Interactor) || !Interactor->IsLocallyControlled()
		|| IsValid(ShopWidget) || !ShopWidgetClass)
	{
		return;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(Interactor->GetController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	PlayerState =
		PlayerController->GetPlayerState<ADRPlayerState>();

	ShopWidget = CreateWidget<UDRShopWidget>(
		PlayerController,
		ShopWidgetClass);

	if (IsValid(ShopWidget))
	{
		ShopWidget->InitializeShop(ItemDefinitions);
		// 위젯을 표시하고 입력을 UI로 전환합니다.
		ShopWidget->OnCloseRequested.AddDynamic(
			this,
			&ThisClass::HideShopWidget);
		ShopWidget->OnPurchaseRequested.AddDynamic(
			this,
			&ThisClass::HandlePurchaseRequested);
		ShopWidget->OnSellAllOresRequested.AddDynamic(
			this,
			&ThisClass::HandleSellAllOresRequested);
		ShopWidget->AddToViewport();

		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(
			EMouseLockMode::DoNotLock);
		// 상호작용 키가 UI에 남아 있는 상태를 초기화합니다.
		PlayerController->FlushPressedKeys();
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void UDRShopUIComponent::HandleInteractionExited(APawn* Interactor)
{
	if (IsValid(Interactor) && Interactor->IsLocallyControlled())
	{
		HideShopWidget();
	}
}

void UDRShopUIComponent::HideShopWidget()
{
	if (!IsValid(ShopWidget))
	{
		return;
	}

	APlayerController* PlayerController = ShopWidget->GetOwningPlayer();

	ShopWidget->OnCloseRequested.RemoveDynamic(
		this,
		&ThisClass::HideShopWidget);
	ShopWidget->OnPurchaseRequested.RemoveDynamic(
		this,
		&ThisClass::HandlePurchaseRequested);
	ShopWidget->OnSellAllOresRequested.RemoveDynamic(
		this,
		&ThisClass::HandleSellAllOresRequested);
	ShopWidget->RemoveFromParent();
	ShopWidget = nullptr;
	PlayerState = nullptr;

	if (IsValid(PlayerController))
	{
		// 위젯을 닫고 게임 입력으로 복구합니다.
		PlayerController->FlushPressedKeys();
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}

void UDRShopUIComponent::HandlePurchaseRequested(
	UDRItemDefinition* ItemDefinition)
{
	if (IsValid(PlayerState))
	{
		// 로컬 UI의 구매 요청을 PlayerState의 서버 RPC로 전달합니다.
		PlayerState->RequestPurchase(GetOwner(), ItemDefinition);
	}
}

void UDRShopUIComponent::HandleSellAllOresRequested()
{
	if (IsValid(PlayerState))
	{
		PlayerState->RequestSellAllOres(GetOwner());
	}
}
