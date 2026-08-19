#include "DRShopComponent.h"

#include "DRShopAreaComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"

UDRShopComponent::UDRShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRShopComponent::SetItemTable(UDataTable* NewItemTable)
{
	if (!IsValid(NewItemTable) || ItemTable == NewItemTable)
	{
		return;
	}

	ItemTable = NewItemTable;
	LoadItemOffers();
}

bool UDRShopComponent::HasItemTable() const
{
	return IsValid(ItemTable);
}

const TArray<FDRShopItemOffer>& UDRShopComponent::GetItemOffers() const
{
	return ItemOffers;
}

bool UDRShopComponent::GetItemRow(
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

bool UDRShopComponent::GetPerkDefinition(
	FName RowName,
	UDRPerkDefinition*& OutPerkDefinition) const
{
	FDRShopItemTableRow ItemRow;
	OutPerkDefinition = nullptr;

	if (!GetItemRow(RowName, ItemRow))
	{
		return false;
	}

	OutPerkDefinition = Cast<UDRPerkDefinition>(ItemRow.ItemDefinition);
	return IsValid(OutPerkDefinition);
}

bool UDRShopComponent::CanPurchasePerk(
	FName RowName,
	const UDRPerkComponent* PerkComponent,
	int32 AvailableCoins) const
{
	UDRPerkDefinition* PerkDefinition = nullptr;

	if (!IsValid(PerkComponent)
		|| !GetPerkDefinition(RowName, PerkDefinition)
		|| !PerkComponent->CanAddTestPerk(PerkDefinition))
	{
		return false;
	}

	return PerkDefinition->Price >= 0
		&& AvailableCoins >= PerkDefinition->Price;
}

bool UDRShopComponent::IsItemAvailable(
	const UDRItemDefinition* ItemDefinition) const
{
	return IsValid(ItemDefinition)
		&& ItemOffers.ContainsByPredicate(
			[ItemDefinition](const FDRShopItemOffer& ItemOffer)
			{
				return ItemOffer.OfferType == EDRShopOfferType::Purchase
					&& ItemOffer.ItemDefinition == ItemDefinition;
			});
}

bool UDRShopComponent::CanPurchase(
	const APawn* Pawn,
	const UDRItemDefinition* ItemDefinition) const
{
	return IsItemAvailable(ItemDefinition)
		&& IsTransactionAllowed(Pawn);
}

bool UDRShopComponent::IsTransactionAllowed(const APawn* Pawn) const
{
	return IsValid(ShopAreaComponent)
		&& IsValid(Pawn)
		&& ShopAreaComponent->IsOverlappingActor(Pawn);
}

void UDRShopComponent::BeginPlay()
{
	Super::BeginPlay();

	ShopAreaComponent =
		GetOwner()->FindComponentByClass<UDRShopAreaComponent>();
	LoadItemOffers();
}

void UDRShopComponent::LoadItemOffers()
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

void UDRShopComponent::AddItemOffers(
	FName RowName,
	const FDRShopItemTableRow& ItemRow)
{
	if (IsValid(Cast<UDRPerkDefinition>(ItemRow.ItemDefinition)))
	{
		FDRShopItemOffer& PerkOffer = ItemOffers.AddDefaulted_GetRef();
		PerkOffer.RowName = RowName;
		PerkOffer.OfferType = EDRShopOfferType::Perk;
		PerkOffer.ItemDefinition = ItemRow.ItemDefinition;
		return;
	}

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
