#include "DRShopUIComponent.h"

#include "DRShopAreaComponent.h"
#include "DRShopComponent.h"
#include "DRShopTransactionComponent.h"
#include "DRUpgradeComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRShopUIComponent::BeginPlay()
{
	Super::BeginPlay();

	ShopAreaComponent =
		GetOwner()->FindComponentByClass<UDRShopAreaComponent>();
	ShopComponent = GetOwner()->FindComponentByClass<UDRShopComponent>();
	UpgradeComponent = GetOwner()->FindComponentByClass<UDRUpgradeComponent>();

	if (!IsValid(ShopAreaComponent)
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent))
	{
		return;
	}

	// 상점 범위 진입과 이탈에 맞춰 상호작용 가능 상태를 변경한다.
	ShopAreaComponent->OnPawnEntered.AddDynamic(
		this,
		&ThisClass::HandlePawnEntered);
	ShopAreaComponent->OnPawnExited.AddDynamic(
		this,
		&ThisClass::HandlePawnExited);
}

void UDRShopUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ShopAreaComponent))
	{
		ShopAreaComponent->OnPawnEntered.RemoveDynamic(
			this,
			&ThisClass::HandlePawnEntered);
		ShopAreaComponent->OnPawnExited.RemoveDynamic(
			this,
			&ThisClass::HandlePawnExited);
	}

	HideShopWidget();
	Super::EndPlay(EndPlayReason);
}

void UDRShopUIComponent::HandlePawnEntered(APawn* Pawn)
{
	if (!IsValid(Pawn)
		|| !Pawn->IsLocallyControlled())
	{
		return;
	}

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Pawn->GetController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	// 입력을 처리할 로컬 플레이어에게 현재 상점을 등록한다.
	PlayerController->SetAvailableShop(this);
}

void UDRShopUIComponent::ToggleShopWidget()
{
	if (IsValid(ShopWidget))
	{
		HideShopWidget();
		return;
	}

	ShowShopWidget();
}

void UDRShopUIComponent::ShowShopWidget()
{
	if (IsValid(ShopWidget)
		|| !ShopWidgetClass
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent))
	{
		return;
	}

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(GetWorld()->GetFirstPlayerController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	ShopTransactionComponent =
		PlayerController->GetShopTransactionComponent();
	InventoryComponent = PlayerController->GetInventoryComponent();
	PlayerState = PlayerController->GetPlayerState<ADRPlayerState>();
	PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;

	if (!IsValid(ShopTransactionComponent)
		|| !IsValid(InventoryComponent)
		|| !IsValid(PerkComponent))
	{
		return;
	}

	ShopWidget = CreateWidget<UDRShopWidget>(
		PlayerController,
		ShopWidgetClass);

	if (!IsValid(ShopWidget))
	{
		return;
	}

	// 위젯에 상점 데이터를 전달하고 UI 요청 이벤트를 연결한다.
	ShopWidget->InitializeShop(MakeOfferViews(
		ShopComponent->GetItemOffers(),
		EDRShopOfferType::Purchase));
	RefreshUpgradeOffers();
	RefreshPerkOffers();
	ShopWidget->OnCloseRequested.AddDynamic(
		this,
		&ThisClass::HideShopWidget);
	ShopWidget->OnOfferRequested.AddDynamic(
		this,
		&ThisClass::HandleOfferRequested);
	ShopWidget->OnSellAllOresRequested.AddDynamic(
		this,
		&ThisClass::HandleSellAllOresRequested);
	InventoryComponent->OnInventoryChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleInventoryChanged);
	PerkComponent->OnPerksChanged.AddDynamic(
		this,
		&ThisClass::HandlePerksChanged);
	PlayerState->OnCoinsChanged.AddDynamic(
		this,
		&ThisClass::HandleCoinsChanged);
	ShopWidget->AddToViewport();

	// 상점 UI를 조작할 수 있도록 마우스와 입력 모드를 전환한다.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->FlushPressedKeys();

	if (!IsMoveInputBlocked)
	{
		PlayerController->SetIgnoreMoveInput(true);
		IsMoveInputBlocked = true;
	}

	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;
}

void UDRShopUIComponent::HandlePawnExited(APawn* Pawn)
{
	if (IsValid(Pawn) && Pawn->IsLocallyControlled())
	{
		if (ADRPlayerController* PlayerController =
			Cast<ADRPlayerController>(Pawn->GetController()))
		{
			PlayerController->ClearAvailableShop(this);
		}

		// 범위를 벗어나면 열려 있는 상점 UI도 함께 닫는다.
		HideShopWidget();
	}
}

void UDRShopUIComponent::HideShopWidget()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}

	if (IsValid(PerkComponent))
	{
		PerkComponent->OnPerksChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePerksChanged);
	}

	if (IsValid(PlayerState))
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCoinsChanged);
	}

	APlayerController* PlayerController = IsValid(ShopWidget)
		? ShopWidget->GetOwningPlayer()
		: nullptr;

	if (IsValid(ShopWidget))
	{
		ShopWidget->OnCloseRequested.RemoveDynamic(
			this,
			&ThisClass::HideShopWidget);
		ShopWidget->OnOfferRequested.RemoveDynamic(
			this,
			&ThisClass::HandleOfferRequested);
		ShopWidget->OnSellAllOresRequested.RemoveDynamic(
			this,
			&ThisClass::HandleSellAllOresRequested);
		ShopWidget->RemoveFromParent();
	}

	ShopWidget = nullptr;
	InventoryComponent = nullptr;
	ShopTransactionComponent = nullptr;
	PerkComponent = nullptr;
	PlayerState = nullptr;

	if (IsValid(PlayerController))
	{
		// 상점 종료 후 게임 입력 상태로 복구한다.
		PlayerController->FlushPressedKeys();

		if (IsMoveInputBlocked)
		{
			PlayerController->SetIgnoreMoveInput(false);
			IsMoveInputBlocked = false;
		}

		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}

	IsMoveInputBlocked = false;
}

void UDRShopUIComponent::HandleOfferRequested(FDRShopOfferRequest Request)
{
	if (IsValid(ShopTransactionComponent))
	{
		ShopTransactionComponent->RequestOffer(GetOwner(), Request);
	}
}

void UDRShopUIComponent::HandleSellAllOresRequested()
{
	if (IsValid(ShopTransactionComponent))
	{
		ShopTransactionComponent->RequestSellAllOres(GetOwner());
	}
}

void UDRShopUIComponent::HandleInventoryChanged()
{
	RefreshUpgradeOffers();
}

void UDRShopUIComponent::HandlePerksChanged()
{
	RefreshPerkOffers();
}

void UDRShopUIComponent::HandleCoinsChanged(int32)
{
	RefreshPerkOffers();
}

void UDRShopUIComponent::RefreshUpgradeOffers()
{
	if (!IsValid(ShopWidget)
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent)
		|| !IsValid(InventoryComponent))
	{
		return;
	}

	ShopWidget->SetUpgradeOffers(MakeOfferViews(
		UpgradeComponent->GetNextUpgradeOffers(
			ShopComponent,
			InventoryComponent),
		EDRShopOfferType::Upgrade));
}

void UDRShopUIComponent::RefreshPerkOffers()
{
	if (IsValid(ShopWidget))
	{
		ShopWidget->SetPerkOffers(BuildPerkOfferViews());
	}
}

TArray<FDRShopOfferView> UDRShopUIComponent::MakeOfferViews(
	const TArray<FDRShopItemOffer>& Offers,
	EDRShopOfferType OfferType) const
{
	TArray<FDRShopOfferView> OfferViews;

	for (const FDRShopItemOffer& Offer : Offers)
	{
		if (Offer.OfferType != OfferType
			|| !IsValid(Offer.ItemDefinition))
		{
			continue;
		}

		FDRShopOfferView& OfferView = OfferViews.AddDefaulted_GetRef();
		OfferView.Request = Offer.MakeRequest();
		OfferView.Request.OfferType = OfferType;
		OfferView.Section = OfferType == EDRShopOfferType::Upgrade
			? EDRShopOfferSection::Upgrade
			: Offer.ItemDefinition->Category == EDRItemCategory::Consumable
				? EDRShopOfferSection::Consumable
				: EDRShopOfferSection::Equipment;
		OfferView.DisplayName = Offer.ItemDefinition->DisplayName;

		if (OfferType == EDRShopOfferType::Upgrade
			&& IsValid(Offer.UpgradeSourceDefinition))
		{
			OfferView.DisplayName = FText::Format(
				FText::FromString(TEXT("{0} → {1}")),
				Offer.UpgradeSourceDefinition->DisplayName,
				Offer.ItemDefinition->DisplayName);
		}

		OfferView.Description = Offer.ItemDefinition->Description;
		OfferView.Icon = Offer.ItemDefinition->Icon;
		OfferView.Price = Offer.ItemDefinition->Price;
	}

	return OfferViews;
}

TArray<FDRShopOfferView> UDRShopUIComponent::BuildPerkOfferViews() const
{
	TArray<FDRShopOfferView> OfferViews;

	// 퍽 상품과 플레이어 구매 상태를 모두 확인할 수 있을 때만 View를 생성한다.
	if (!IsValid(ShopComponent)
		|| !IsValid(PerkComponent)
		|| !IsValid(PlayerState))
	{
		return OfferViews;
	}

	for (const FDRShopItemOffer& Offer : ShopComponent->GetItemOffers())
	{
		// 일반 상품과 장비 업그레이드는 퍽 UI에서 제외한다.
		if (Offer.OfferType != EDRShopOfferType::Perk)
		{
			continue;
		}

		// 상점 ItemDefinition이 실제 퍽 Definition인지 확인한다.
		UDRPerkDefinition* PerkDefinition =
			Cast<UDRPerkDefinition>(Offer.ItemDefinition);

		if (!IsValid(PerkDefinition))
		{
			continue;
		}

		// 퍽 Definition의 표시 데이터로 상점 UI View를 구성한다.
		FDRShopOfferView& OfferView = OfferViews.AddDefaulted_GetRef();
		OfferView.Request = Offer.MakeRequest();
		OfferView.Request.OfferType = EDRShopOfferType::Perk;
		OfferView.Section = EDRShopOfferSection::Perk;
		OfferView.DisplayName = FText::Format(
			FText::FromString(TEXT("{0} 퍽")),
			PerkDefinition->DisplayName);
		OfferView.Description = PerkDefinition->Description;
		OfferView.Icon = PerkDefinition->Icon;
		OfferView.Price = PerkDefinition->Price;
		// 현재 코인과 전체 퍽 슬롯 제한을 기준으로 버튼 활성 상태를 결정한다.
		OfferView.IsPurchasable = ShopComponent->CanPurchasePerk(
			Offer.RowName,
			PerkComponent,
			PlayerState->GetCoins());
	}

	return OfferViews;
}
