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
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ShopWidgetLayer = EDRUILayer::Menu;
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
	PlayerController = nullptr;
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UDRShopUIComponent::HandlePawnEntered(APawn* Pawn)
{
	if (!IsValid(Pawn)
		|| !Pawn->IsLocallyControlled())
	{
		return;
	}

	ADRPlayerController* NewPlayerController =
		Cast<ADRPlayerController>(Pawn->GetController());

	if (!IsValid(NewPlayerController)
		|| !NewPlayerController->IsLocalController())
	{
		return;
	}

	// 입력을 처리할 로컬 플레이어에게 현재 상점을 등록한다.
	PlayerController = NewPlayerController;

	if (ULocalPlayer* LocalPlayer = NewPlayerController->GetLocalPlayer())
	{
		UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	}

	NewPlayerController->SetAvailableShop(this);
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

	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(UIManager))
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

	ShopWidget = Cast<UDRShopWidget>(UIManager->CreateManagedWidget(
		ShopWidgetClass,
		ShopWidgetLayer));

	if (!IsValid(ShopWidget))
	{
		return;
	}

	// 위젯에 상점 데이터를 전달하고 UI 요청 이벤트를 연결한다.
	ShopWidget->SetOffers(
		EDRShopOfferType::Purchase,
		MakeOfferViews(
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
	InventoryComponent->OnInventoryChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleInventoryChanged);
	PerkComponent->OnPerksChanged.AddDynamic(
		this,
		&ThisClass::HandlePerksChanged);
	PlayerState->OnCoinsChanged.AddDynamic(
		this,
		&ThisClass::HandleCoinsChanged);
	// 상점 UI를 조작하는 동안 캐릭터 이동만 차단한다.
	PlayerController->FlushPressedKeys();

	if (!IsMoveInputBlocked)
	{
		PlayerController->SetIgnoreMoveInput(true);
		IsMoveInputBlocked = true;
	}

}

void UDRShopUIComponent::HandlePawnExited(APawn* Pawn)
{
	if (IsValid(Pawn) && Pawn->IsLocallyControlled())
	{
		if (ADRPlayerController* ExitingPlayerController =
			Cast<ADRPlayerController>(Pawn->GetController()))
		{
			ExitingPlayerController->ClearAvailableShop(this);
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

	if (IsValid(ShopWidget))
	{
		ShopWidget->OnCloseRequested.RemoveDynamic(
			this,
			&ThisClass::HideShopWidget);
		ShopWidget->OnOfferRequested.RemoveDynamic(
			this,
			&ThisClass::HandleOfferRequested);

		if (IsValid(UIManager))
		{
			UIManager->ReleaseManagedWidget(ShopWidget);
		}
		else
		{
			ShopWidget->RemoveFromParent();
		}
	}

	ShopWidget = nullptr;
	InventoryComponent = nullptr;
	ShopTransactionComponent = nullptr;
	PerkComponent = nullptr;
	PlayerState = nullptr;

	if (IsValid(PlayerController))
	{
		// 상점 종료 후 이동 입력을 복구한다.
		PlayerController->FlushPressedKeys();

		if (IsMoveInputBlocked)
		{
			PlayerController->SetIgnoreMoveInput(false);
			IsMoveInputBlocked = false;
		}

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

void UDRShopUIComponent::HandleInventoryChanged()
{
	RefreshItemOffers();
	RefreshUpgradeOffers();
}

void UDRShopUIComponent::HandlePerksChanged()
{
	RefreshPerkOffers();
}

void UDRShopUIComponent::HandleCoinsChanged(int32)
{
	RefreshItemOffers();
	RefreshUpgradeOffers();
	RefreshPerkOffers();
}

void UDRShopUIComponent::RefreshItemOffers()
{
	if (!IsValid(ShopWidget) || !IsValid(ShopComponent))
	{
		return;
	}

	ShopWidget->SetOffers(
		EDRShopOfferType::Purchase,
		MakeOfferViews(
			ShopComponent->GetItemOffers(),
			EDRShopOfferType::Purchase));
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

	ShopWidget->SetOffers(
		EDRShopOfferType::Upgrade,
		MakeOfferViews(
			UpgradeComponent->GetNextUpgradeOffers(
				ShopComponent,
				InventoryComponent),
			EDRShopOfferType::Upgrade));
}

void UDRShopUIComponent::RefreshPerkOffers()
{
	if (IsValid(ShopWidget) && IsValid(ShopComponent))
	{
		ShopWidget->SetOffers(
			EDRShopOfferType::Perk,
			MakeOfferViews(
				ShopComponent->GetItemOffers(),
				EDRShopOfferType::Perk));
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

		UDRPerkDefinition* PerkDefinition = OfferType == EDRShopOfferType::Perk
			? Cast<UDRPerkDefinition>(Offer.ItemDefinition)
			: nullptr;

		if (OfferType == EDRShopOfferType::Perk
			&& !IsValid(PerkDefinition))
		{
			continue;
		}

		FDRShopOfferView& OfferView = OfferViews.AddDefaulted_GetRef();
		OfferView.Request = Offer.MakeRequest();
		OfferView.Section = OfferType == EDRShopOfferType::Perk
			? EDRShopOfferSection::Perk
			: OfferType == EDRShopOfferType::Upgrade
				? EDRShopOfferSection::Upgrade
				: Offer.ItemDefinition->Category == EDRItemCategory::Consumable
					? EDRShopOfferSection::Consumable
					: EDRShopOfferSection::Equipment;
		OfferView.DisplayName = Offer.ItemDefinition->DisplayName;

		if (IsValid(PerkDefinition))
		{
			OfferView.DisplayName = FText::Format(
				FText::FromString(TEXT("{0} 퍽")),
				PerkDefinition->DisplayName);
		}
		else if (OfferType == EDRShopOfferType::Upgrade
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

		if (!IsValid(ShopComponent) || !IsValid(PlayerState))
		{
			OfferView.IsPurchasable = false;
			continue;
		}

		switch (OfferType)
		{
		case EDRShopOfferType::Purchase:
			OfferView.IsPurchasable = ShopComponent->CanPurchaseItem(
				InventoryComponent,
				Offer.ItemDefinition,
				PlayerState->GetCoins());
			break;

		case EDRShopOfferType::Perk:
			OfferView.IsPurchasable = ShopComponent->CanPurchasePerk(
				PerkDefinition,
				PerkComponent,
				PlayerState->GetCoins());
			break;

		default:
			OfferView.IsPurchasable = ShopComponent->CanAfford(
				Offer.ItemDefinition,
				PlayerState->GetCoins());
			break;
		}
	}

	return OfferViews;
}
