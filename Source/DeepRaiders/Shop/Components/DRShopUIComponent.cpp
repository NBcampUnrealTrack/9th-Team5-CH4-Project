#include "DRShopUIComponent.h"
#include "DeepRaiders/Upgrade/DRCharacterUpgradeComponent.h"
#include "DeepRaiders/Upgrade/DRCharacterUpgradeProfile.h"

#include "DRShopComponent.h"
#include "DRShopTransactionComponent.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Item/Upgrade/DRWeaponUpgradeProfile.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
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

}

void UDRShopUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	HideShopWidget();

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
		|| !IsValid(UIManager))
	{
		return;
	}

	ShopComponent = ShopActor->FindComponentByClass<UDRShopComponent>();

	if (!IsValid(ShopComponent))
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
	UIManager->RegisterCloseHandler(
		ShopWidget, FSimpleDelegate::CreateUObject(this, &ThisClass::HideShopWidget));

	// 위젯에 상점 데이터를 전달하고 UI 요청 이벤트를 연결한다.
	ShopWidget->InitializeInventoryPanels(InventoryComponent, PerkComponent);
	RefreshOffers(EDRShopOfferType::Purchase);
	RefreshOffers(EDRShopOfferType::Perk);
	BindShopEvents();
	RefreshCharacterUpgrades();
	RefreshWeaponUpgrades();
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
	UnbindShopEvents();

	if (IsValid(ShopWidget))
	{
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

void UDRShopUIComponent::BindShopEvents()
{
	if (IsValid(PlayerState))
	{
		PlayerState->GetCharacterUpgradeComponent()->OnUpgradesChanged.AddDynamic(
			this, &ThisClass::RefreshCharacterUpgrades);
	}
	if (IsValid(ShopWidget))
	{
		ShopWidget->OnCloseRequested.AddDynamic(
			this,
			&ThisClass::HideShopWidget);
		ShopWidget->OnOfferRequested.AddDynamic(
			this,
			&ThisClass::HandleOfferRequested);
		ShopWidget->OnSellRequested.AddDynamic(
			this,
			&ThisClass::HandleSellRequested);
	}

	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChangedDelegate.AddDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}

	if (IsValid(PerkComponent))
	{
		PerkComponent->OnPerksChanged.AddDynamic(
			this,
			&ThisClass::HandlePerksChanged);
	}

	if (IsValid(PlayerState))
	{
		if (UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::GetSnowGaugeAttribute()).AddUObject(
					this, &ThisClass::HandleSnowGaugeChanged);
		}
	}
}

void UDRShopUIComponent::UnbindShopEvents()
{
	if (IsValid(PlayerState))
	{
		PlayerState->GetCharacterUpgradeComponent()->OnUpgradesChanged.RemoveDynamic(
			this, &ThisClass::RefreshCharacterUpgrades);
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
	}

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
		if (UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::GetSnowGaugeAttribute()).RemoveAll(this);
		}
	}
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
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=UIForward Reason=MissingTransactionComponent Tag=%s"),
			*Request.UpgradeTag.ToString());
	}
}

void UDRShopUIComponent::HandleSellRequested(
	EDRShopSellTargetType TargetType,
	FGuid InstanceId)
{
	if (IsValid(ShopTransactionComponent))
	{
		if (TargetType == EDRShopSellTargetType::Perk)
		{
			ShopTransactionComponent->RequestSellPerk(ActiveShop.Get(), InstanceId);
		}
		else
		{
			ShopTransactionComponent->RequestSell(ActiveShop.Get(), InstanceId);
		}
	}
}

void UDRShopUIComponent::HandleInventoryChanged()
{
	RefreshOffers(EDRShopOfferType::Purchase);
	RefreshWeaponUpgrades();
}

void UDRShopUIComponent::HandlePerksChanged()
{
	RefreshOffers(EDRShopOfferType::Perk);
}

void UDRShopUIComponent::HandleSnowGaugeChanged(const FOnAttributeChangeData&)
{
	RefreshCharacterUpgrades();
	RefreshWeaponUpgrades();
	RefreshOffers(EDRShopOfferType::Purchase);
	RefreshOffers(EDRShopOfferType::Perk);
}

void UDRShopUIComponent::RefreshOffers(EDRShopOfferType OfferType)
{
	if (!IsValid(ShopWidget) || !IsValid(ShopComponent))
	{
		return;
	}

	ShopWidget->SetOffers(
		OfferType,
		MakeOfferViews(
			ShopComponent->GetItemOffers(),
			OfferType));
}

void UDRShopUIComponent::RefreshCharacterUpgrades()
{
	if (!IsValid(ShopWidget) || !IsValid(PlayerState))
	{
		return;
	}
	const UDRCharacterUpgradeComponent* Component = PlayerState->GetCharacterUpgradeComponent();
	const UDRCharacterUpgradeProfile* Profile = IsValid(Component) ? Component->GetProfile() : nullptr;
	TArray<FDRShopOfferView> Offers;
	if (IsValid(Profile) && Profile->IsUsable())
	{
		for (const FDRStatUpgradeData& Data : Profile->GetStatUpgrades())
		{
			const int32 Level = Component->GetUpgradeLevel(Data.UpgradeTag);
			FDRShopOfferView& Offer = Offers.AddDefaulted_GetRef();
			Offer.Request.OfferType = EDRShopOfferType::CharacterUpgrade;
			Offer.Request.UpgradeTag = Data.UpgradeTag;
			Offer.Request.ExpectedLevel = Level;
			Offer.Section = EDRShopOfferSection::CharacterUpgrade;
			Offer.DisplayName = FText::Format(
				NSLOCTEXT("Shop", "CharacterUpgradeName", "{0} (Lv. {1})"), Data.DisplayName, FText::AsNumber(Level));
			Offer.Description = FText::Format(
				NSLOCTEXT("Shop", "CharacterUpgradeDescription", "{0}\n구매마다 기본 스탯 +{1}% 현재 누적 +{2}%"),
				Data.Description, FText::AsNumber(Data.IncreasePercent),
				FText::AsNumber(Data.GetTotalIncreasePercent(Level)));
			Offer.Icon = Data.Icon;
			Offer.Price = Data.Price;
			Offer.IsPurchasable = Data.IsUpgradeAvailable(Level) && PlayerState->GetSnowGauge() >= Data.Price;
		}
	}
	ShopWidget->SetOffers(EDRShopOfferType::CharacterUpgrade, Offers);
}


void UDRShopUIComponent::RefreshWeaponUpgrades()
{
	if (!IsValid(ShopWidget) || !IsValid(InventoryComponent) || !IsValid(PlayerState))
	{
		return;
	}

	TArray<FDRShopOfferView> Offers;
	for (const FDRItemInstance& Item : InventoryComponent->GetItemInstances())
	{
		const UDRRangedWeaponDefinition* Weapon = Cast<UDRRangedWeaponDefinition>(Item.Definition.Get());
		const FDRSnowProjectileWeaponRuntimeState* State = Item.RuntimeState.GetPtr<FDRSnowProjectileWeaponRuntimeState>();
		UDRWeaponUpgradeProfile* UpgradeProfile =
			IsValid(Weapon) ? Weapon->GetUpgradeProfile() : nullptr;
		if (!Item.IsValid() || !IsValid(Weapon)
			|| !IsValid(UpgradeProfile) || !UpgradeProfile->IsUsable())
		{
			continue;
		}

		if (State == nullptr)
		{
			FDRShopOfferView& Offer = Offers.AddDefaulted_GetRef();
			Offer.Request.OfferType = EDRShopOfferType::WeaponUpgrade;
			Offer.Request.InstanceId = Item.InstanceId;
			Offer.Section = EDRShopOfferSection::WeaponUpgrade;
			Offer.DisplayName = Weapon->DisplayName;
			Offer.WeaponName = Weapon->DisplayName;
			Offer.WeaponIcon = Weapon->Icon;
			Offer.Description = Weapon->Description;
			Offer.Icon = Weapon->Icon;
			Offer.IsPurchasable = false;
			continue;
		}

		for (const FDRWeaponStatUpgradeData& Data : UpgradeProfile->GetStatUpgrades())
		{
			const int32 Level = State->GetUpgradeLevel(Data.UpgradeTag);
			const FDRWeaponUpgradeLevelData* NextLevel = Level < Data.GetMaxLevel() ? Data.FindLevelData(Level + 1) : nullptr;
			FDRShopOfferView& Offer = Offers.AddDefaulted_GetRef();
			Offer.Request.OfferType = EDRShopOfferType::WeaponUpgrade;
			Offer.Request.InstanceId = Item.InstanceId;
			Offer.Request.UpgradeTag = Data.UpgradeTag;
			Offer.Request.ExpectedLevel = Level;
			Offer.Section = EDRShopOfferSection::WeaponUpgrade;
			Offer.WeaponName = Weapon->DisplayName;
			Offer.WeaponIcon = Weapon->Icon;
			Offer.DisplayName = Data.DisplayName;
			Offer.MaxLevel = Data.GetMaxLevel();
			Offer.Description = NextLevel != nullptr
				? FText::Format(NSLOCTEXT("Shop", "WeaponUpgradeNext", "{0}\n기본값 대비 {1}% → {2}%"),
					Data.Description, FText::AsNumber(Data.GetStatMultiplier(Level) * 100.f),
					FText::AsNumber(NextLevel->SetByCallerMagnitude * 100.f))
				: FText::Format(NSLOCTEXT("Shop", "WeaponUpgradeMax", "{0}\n최대 레벨 · 기본값 대비 {1}%"),
					Data.Description, FText::AsNumber(Data.GetStatMultiplier(Level) * 100.f));
			Offer.Icon = IsValid(Data.Icon) ? Data.Icon : Weapon->Icon;
			Offer.Price = NextLevel != nullptr ? NextLevel->Price : 0;
			Offer.IsPurchasable = NextLevel != nullptr && PlayerState->GetSnowGauge() >= NextLevel->Price;
		}
	}
	ShopWidget->SetOffers(EDRShopOfferType::WeaponUpgrade, Offers);
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
		if (OfferType == EDRShopOfferType::Perk
			&& (!IsValid(PerkComponent)
				|| !PerkComponent->IsCompatibleWithEquippedSkills(PerkDefinition)))
		{
			continue;
		}

		FDRShopOfferView& OfferView = OfferViews.AddDefaulted_GetRef();
		OfferView.Request = Offer.MakeRequest();
		OfferView.Section = ResolveOfferSection(Offer);
		OfferView.DisplayName = Offer.ItemDefinition->DisplayName;
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
				PlayerState->GetSnowGauge());
			break;

		case EDRShopOfferType::Perk:
			OfferView.IsPurchasable = ShopComponent->CanPurchasePerk(
				PerkDefinition,
				PerkComponent,
				PlayerState->GetSnowGauge());
			break;

		default:
			OfferView.IsPurchasable = ShopComponent->CanAfford(
				Offer.ItemDefinition,
				PlayerState->GetSnowGauge());
			break;
		}
	}

	return OfferViews;
}

EDRShopOfferSection UDRShopUIComponent::ResolveOfferSection(
	const FDRShopItemOffer& Offer) const
{
	switch (Offer.OfferType)
	{
	case EDRShopOfferType::Perk:
		return EDRShopOfferSection::Perk;

	default:
		return IsValid(Offer.ItemDefinition)
			&& Offer.ItemDefinition->Category == EDRItemCategory::Consumable
			? EDRShopOfferSection::Consumable
			: EDRShopOfferSection::Equipment;
	}
}
