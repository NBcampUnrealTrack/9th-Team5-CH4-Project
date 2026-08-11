#include "DRShopUIComponent.h"

#include "DRInteractionComponent.h"
#include "DRUpgradeComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const TArray<FDRShopItemOffer>& UDRShopUIComponent::GetItemOffers() const
{
	return ItemOffers;
}

bool UDRShopUIComponent::GetItemRow(
	FName RowName,
	FDRShopItemTableRow& OutItemRow) const
{
	if (!IsValid(ItemTable) || RowName.IsNone())
	{
		return false;
	}

	const FDRShopItemTableRow* ItemRow =
		ItemTable->FindRow<FDRShopItemTableRow>(RowName, TEXT("GetItemRow"));

	if (!ItemRow || !IsValid(ItemRow->ItemDefinition))
	{
		return false;
	}

	OutItemRow = *ItemRow;
	return true;
}

bool UDRShopUIComponent::IsItemAvailable(
	const UDRItemDefinition* ItemDefinition) const
{
	return IsValid(ItemDefinition)
		&& ItemOffers.ContainsByPredicate(
			[ItemDefinition](const FDRShopItemOffer& ItemOffer)
			{
				return !ItemOffer.IsUpgrade()
					&& ItemOffer.ItemDefinition == ItemDefinition;
			});
}

bool UDRShopUIComponent::IsTransactionAllowed(const APawn* Interactor) const
{
	return IsValid(InteractionComponent)
		&& IsValid(Interactor)
		&& InteractionComponent->IsOverlappingActor(Interactor);
}

void UDRShopUIComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadItemOffers();

	InteractionComponent =
		GetOwner()->FindComponentByClass<UDRInteractionComponent>();
	UpgradeComponent =
		GetOwner()->FindComponentByClass<UDRUpgradeComponent>();

	if (!IsValid(InteractionComponent) || !IsValid(UpgradeComponent))
	{
		return;
	}

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

void UDRShopUIComponent::LoadItemOffers()
{
	ItemOffers.Reset();

	if (!IsValid(ItemTable))
	{
		return;
	}

	for (const FName RowName : ItemTable->GetRowNames())
	{
		const FDRShopItemTableRow* ItemRow =
			ItemTable->FindRow<FDRShopItemTableRow>(
				RowName,
				TEXT("LoadItemOffers"));

		if (ItemRow && IsValid(ItemRow->ItemDefinition))
		{
			AddItemOffers(RowName, *ItemRow);
		}
	}
}

void UDRShopUIComponent::AddItemOffers(
	FName RowName,
	const FDRShopItemTableRow& ItemRow)
{
	if (!ItemRow.IsUpgradeRow())
	{
		FDRShopItemOffer& ItemOffer = ItemOffers.AddDefaulted_GetRef();
		ItemOffer.RowName = RowName;
		ItemOffer.ItemDefinition = ItemRow.ItemDefinition;
		return;
	}

	for (int32 TargetLevel = 1;
		TargetLevel <= ItemRow.GetMaxUpgradeLevel();
		++TargetLevel)
	{
		UDRItemDefinition* SourceDefinition =
			ItemRow.GetUpgradeSourceDefinition(TargetLevel);
		UDRItemDefinition* TargetDefinition =
			ItemRow.GetUpgradeTargetDefinition(TargetLevel);

		if (!IsValid(TargetDefinition)
			|| (TargetLevel > 1 && !IsValid(SourceDefinition)))
		{
			continue;
		}

		FDRShopItemOffer& ItemOffer = ItemOffers.AddDefaulted_GetRef();
		ItemOffer.RowName = RowName;
		ItemOffer.OfferType = EDRShopOfferType::Upgrade;
		ItemOffer.ItemDefinition = TargetDefinition;
		ItemOffer.UpgradeSourceDefinition = SourceDefinition;
		ItemOffer.TargetLevel = TargetLevel;
	}
}

void UDRShopUIComponent::HandleInteractionEntered(APawn* Interactor)
{
	if (!IsValid(Interactor)
		|| !Interactor->IsLocallyControlled()
		|| IsValid(ShopWidget)
		|| !ShopWidgetClass
		|| !IsValid(UpgradeComponent))
	{
		return;
	}

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Interactor->GetController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	PlayerState = PlayerController->GetPlayerState<ADRPlayerState>();
	InventoryComponent = PlayerController->GetQuickSlotInventoryComponent();

	if (!IsValid(PlayerState) || !IsValid(InventoryComponent))
	{
		return;
	}

	ShopWidget = CreateWidget<UDRShopWidget>(PlayerController, ShopWidgetClass);

	if (!IsValid(ShopWidget))
	{
		return;
	}

	ShopWidget->InitializeShop(ItemOffers);
	RefreshUpgradeOffers();
	ShopWidget->OnCloseRequested.AddDynamic(this, &ThisClass::HideShopWidget);
	ShopWidget->OnOfferRequested.AddDynamic(
		this,
		&ThisClass::HandleOfferRequested);
	ShopWidget->OnSellAllOresRequested.AddDynamic(
		this,
		&ThisClass::HandleSellAllOresRequested);
	InventoryComponent->OnInventoryChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleInventoryChanged);
	ShopWidget->AddToViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->FlushPressedKeys();
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;
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
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
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
	PlayerState = nullptr;

	if (IsValid(PlayerController))
	{
		PlayerController->FlushPressedKeys();
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}

void UDRShopUIComponent::HandleOfferRequested(FDRShopOfferRequest Request)
{
	if (IsValid(PlayerState))
	{
		PlayerState->RequestOffer(GetOwner(), Request);
	}
}

void UDRShopUIComponent::HandleSellAllOresRequested()
{
	if (IsValid(PlayerState))
	{
		PlayerState->RequestSellAllOres(GetOwner());
	}
}

void UDRShopUIComponent::HandleInventoryChanged()
{
	RefreshUpgradeOffers();
}

void UDRShopUIComponent::RefreshUpgradeOffers()
{
	if (!IsValid(ShopWidget)
		|| !IsValid(UpgradeComponent)
		|| !IsValid(InventoryComponent))
	{
		return;
	}

	ShopWidget->SetUpgradeOffers(
		UpgradeComponent->GetNextUpgradeOffers(this, InventoryComponent));
}
