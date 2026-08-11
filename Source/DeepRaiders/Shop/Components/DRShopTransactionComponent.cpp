#include "DRShopTransactionComponent.h"

#include "DRShopComponent.h"
#include "DRUpgradeComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRShopTransactionComponent::UDRShopTransactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRShopTransactionComponent::RequestOffer(
	AActor* ShopActor,
	const FDRShopOfferRequest& Request)
{
	if (!IsValid(ShopActor)
		|| Request.RowName.IsNone()
		|| (Request.OfferType == EDRShopOfferType::Upgrade
			&& Request.TargetLevel <= 0))
	{
		return;
	}

	ServerRequestOffer(ShopActor, Request);
}

void UDRShopTransactionComponent::RequestSellAllOres(AActor* ShopActor)
{
	if (IsValid(ShopActor))
	{
		ServerSellAllOres(ShopActor);
	}
}

void UDRShopTransactionComponent::ServerRequestOffer_Implementation(
	AActor* ShopActor,
	FDRShopOfferRequest Request)
{
	ADRPlayerState* PlayerState = GetPlayerState();
	const UDRShopComponent* ShopComponent = IsValid(ShopActor)
		? ShopActor->FindComponentByClass<UDRShopComponent>()
		: nullptr;
	UDRInventoryComponent* Inventory = GetInventoryComponent();
	FDRShopItemTableRow ItemRow;

	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(Inventory)
		|| Request.RowName.IsNone()
		|| !ShopComponent->IsTransactionAllowed(PlayerState->GetPawn())
		|| !ShopComponent->GetItemRow(Request.RowName, ItemRow))
	{
		return;
	}

	switch (Request.OfferType)
	{
	case EDRShopOfferType::Purchase:
		TryPurchase(PlayerState, ShopComponent, Inventory, ItemRow);
		break;

	case EDRShopOfferType::Upgrade:
		TryUpgrade(
			PlayerState,
			ShopActor->FindComponentByClass<UDRUpgradeComponent>(),
			Inventory,
			ItemRow,
			Request.TargetLevel);
		break;
	}
}

void UDRShopTransactionComponent::ServerSellAllOres_Implementation(
	AActor* ShopActor)
{
	ADRPlayerState* PlayerState = GetPlayerState();
	const UDRShopComponent* ShopComponent = IsValid(ShopActor)
		? ShopActor->FindComponentByClass<UDRShopComponent>()
		: nullptr;
	UDRInventoryComponent* Inventory = GetInventoryComponent();

	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(Inventory)
		|| !ShopComponent->IsTransactionAllowed(PlayerState->GetPawn()))
	{
		return;
	}

	TArray<FGuid> EntryIds;
	int32 TotalQuantity = 0;
	const int64 TotalPrice = CollectSellableOreEntries(
		Inventory,
		EntryIds,
		TotalQuantity);

	if (EntryIds.IsEmpty()
		|| TotalPrice <= 0
		|| TotalPrice > static_cast<int64>(MAX_int32) - PlayerState->GetCoins()
		|| !Inventory->TryRemoveEntries(EntryIds))
	{
		return;
	}

	PlayerState->SetCoins(
		PlayerState->GetCoins() + static_cast<int32>(TotalPrice));
}

ADRPlayerState* UDRShopTransactionComponent::GetPlayerState() const
{
	const ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(GetOwner());

	return IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADRPlayerState>()
		: nullptr;
}

UDRInventoryComponent* UDRShopTransactionComponent::GetInventoryComponent() const
{
	const ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(GetOwner());

	return IsValid(PlayerController)
		? PlayerController->GetInventoryComponent()
		: nullptr;
}

bool UDRShopTransactionComponent::TryPurchase(
	ADRPlayerState* PlayerState,
	const UDRShopComponent* ShopComponent,
	UDRInventoryComponent* Inventory,
	const FDRShopItemTableRow& ItemRow) const
{
	UDRItemDefinition* ItemDefinition = ItemRow.ItemDefinition;

	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(Inventory)
		|| ItemRow.IsUpgradeRow()
		|| !IsValid(ItemDefinition)
		|| ItemDefinition->Price < 0
		|| !ShopComponent->IsItemAvailable(ItemDefinition)
		|| PlayerState->GetCoins() < ItemDefinition->Price
		|| !Inventory->CanAddItem(ItemDefinition, 1)
		|| !Inventory->TryAddItem(ItemDefinition, 1))
	{
		return false;
	}

	PlayerState->SetCoins(PlayerState->GetCoins() - ItemDefinition->Price);
	return true;
}

bool UDRShopTransactionComponent::TryUpgrade(
	ADRPlayerState* PlayerState,
	const UDRUpgradeComponent* UpgradeComponent,
	UDRInventoryComponent* Inventory,
	const FDRShopItemTableRow& ItemRow,
	int32 TargetLevel) const
{
	FDRUpgradeOperation Operation;

	if (!IsValid(PlayerState)
		|| !IsValid(UpgradeComponent)
		|| !IsValid(Inventory)
		|| !UpgradeComponent->BuildUpgradeOperation(
			ItemRow,
			TargetLevel,
			Inventory,
			Operation)
		|| !IsValid(Operation.TargetDefinition)
		|| Operation.TargetDefinition->Price < 0
		|| PlayerState->GetCoins() < Operation.TargetDefinition->Price
		|| !UpgradeComponent->ApplyUpgrade(Inventory, Operation))
	{
		return false;
	}

	PlayerState->SetCoins(
		PlayerState->GetCoins() - Operation.TargetDefinition->Price);
	return true;
}

int64 UDRShopTransactionComponent::CollectSellableOreEntries(
	const UDRInventoryComponent* Inventory,
	TArray<FGuid>& OutEntryIds,
	int32& OutTotalQuantity) const
{
	OutEntryIds.Reset();
	OutTotalQuantity = 0;
	int64 TotalPrice = 0;

	if (!IsValid(Inventory))
	{
		return TotalPrice;
	}

	for (const FDRInventoryEntry& Entry : Inventory->GetEntries())
	{
		const UDRItemDefinition* Definition = Entry.Definition;

		if (!Entry.IsValid()
			|| !IsValid(Definition)
			|| Definition->Category != EItemCategory::Ore
			|| !Definition->bCanBeSold
			|| Definition->Price <= 0)
		{
			continue;
		}

		const int64 EntryPrice =
			static_cast<int64>(Definition->Price) * Entry.Quantity;

		if (Entry.Quantity > MAX_int32 - OutTotalQuantity
			|| EntryPrice > MAX_int64 - TotalPrice)
		{
			OutEntryIds.Reset();
			OutTotalQuantity = 0;
			return -1;
		}

		OutEntryIds.Add(Entry.EntryId);
		OutTotalQuantity += Entry.Quantity;
		TotalPrice += EntryPrice;
	}

	return TotalPrice;
}
