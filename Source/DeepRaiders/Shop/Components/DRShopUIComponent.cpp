#include "DRShopUIComponent.h"

#include "DRShopComponent.h"
#include "DRShopTransactionComponent.h"
#include "DRUpgradeComponent.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/Components/DRStartingWeaponSelectionComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "Engine/LocalPlayer.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRShopUIComponent::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	}

	StartingWeaponSelectionComponent =
		PlayerController->GetStartingWeaponSelectionComponent();

	// 게임플레이 컴포넌트가 UI를 직접 참조하지 않도록 상태 이벤트만 구독한다.
	if (IsValid(StartingWeaponSelectionComponent))
	{
		StartingWeaponSelectionComponent->OnSelectionAvailabilityChanged.AddUObject(
			this,
			&ThisClass::HandleStartingWeaponSelectionAvailabilityChanged);
	}
}

void UDRShopUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	HideShopWidget();

	if (IsValid(StartingWeaponSelectionComponent))
	{
		StartingWeaponSelectionComponent->OnSelectionAvailabilityChanged.RemoveAll(this);
	}

	StartingWeaponSelectionComponent = nullptr;
	PlayerController = nullptr;
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UDRShopUIComponent::ToggleShopWidget(AActor* ShopActor)
{
	if (IsValid(ShopWidget))
	{
		HideShopWidget();
		return;
	}

	ShowShopWidget(ShopActor);
}

void UDRShopUIComponent::CloseShop(const AActor* ShopActor)
{
	if (ActiveShop.Get() == ShopActor)
	{
		HideShopWidget();
	}
}

void UDRShopUIComponent::ShowShopWidget(AActor* ShopActor)
{
	if (IsValid(ShopWidget) || !IsValid(ShopActor))
	{
		return;
	}

	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(StartingWeaponSelectionComponent)
		|| !IsValid(UIManager))
	{
		return;
	}

	ShopComponent = ShopActor->FindComponentByClass<UDRShopComponent>();
	UpgradeComponent = ShopActor->FindComponentByClass<UDRUpgradeComponent>();

	if (!IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent))
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

	ShopWidget = Cast<UDRShopWidget>(UIManager->PushScreen(DRGameplayTags::UI_Screen_Shop));

	if (!IsValid(ShopWidget))
	{
		return;
	}

	ActiveShop = ShopActor;

	// 위젯에 상점 데이터를 전달하고 UI 요청 이벤트를 연결한다.
	ShopWidget->SetOffers(
		EDRShopOfferType::Purchase,
		MakeOfferViews(
			ShopComponent->GetItemOffers(),
			EDRShopOfferType::Purchase));
	ShopWidget->InitializeSellPanel(InventoryComponent);
	// 선택 가능 상태라면 상점이 열릴 때 최초 무기 탭을 우선 표시한다.
	ShopWidget->InitializeStartingWeaponPanel(StartingWeaponSelectionComponent);
	RefreshUpgradeOffers();
	RefreshPerkOffers();
	ShopWidget->OnCloseRequested.AddDynamic(
		this,
		&ThisClass::HideShopWidget);
	ShopWidget->OnOfferRequested.AddDynamic(
		this,
		&ThisClass::HandleOfferRequested);
	ShopWidget->OnSellRequested.AddDynamic(
		this,
		&ThisClass::HandleSellRequested);
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

	SetShopOpenTag(true);
}

void UDRShopUIComponent::HideShopWidget()
{
	SetShopOpenTag(false);

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
		ShopWidget->OnSellRequested.RemoveDynamic(
			this,
			&ThisClass::HandleSellRequested);

		if (IsValid(UIManager))
		{
			UIManager->PopScreen(DRGameplayTags::UI_Screen_Shop);
		}
		else
		{
			ShopWidget->RemoveFromParent();
		}
	}

	ShopWidget = nullptr;
	ActiveShop = nullptr;
	InventoryComponent = nullptr;
	ShopTransactionComponent = nullptr;
	ShopComponent = nullptr;
	UpgradeComponent = nullptr;
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

void UDRShopUIComponent::SetShopOpenTag(bool bIsOpen) const
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = PlayerController->GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(
			DRGameplayTags::State_UI_ShopOpen,
			bIsOpen ? 1 : 0);
	}
}

void UDRShopUIComponent::HandleOfferRequested(FDRShopOfferRequest Request)
{
	if (IsValid(ShopTransactionComponent))
	{
		ShopTransactionComponent->RequestOffer(ActiveShop.Get(), Request);
	}
}

void UDRShopUIComponent::HandleSellRequested(FGuid InstanceId)
{
	if (IsValid(ShopTransactionComponent))
	{
		ShopTransactionComponent->RequestSell(ActiveShop.Get(), InstanceId);
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

void UDRShopUIComponent::HandleStartingWeaponSelectionAvailabilityChanged(
	bool IsAvailable)
{
	if (!IsAvailable && IsValid(ShopWidget))
	{
		ShopWidget->DisableStartingWeaponPanel();
	}
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
