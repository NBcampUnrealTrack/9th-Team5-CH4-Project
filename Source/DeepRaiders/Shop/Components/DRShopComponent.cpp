#include "DRShopComponent.h"

#include "DRShopAreaComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
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

bool UDRShopComponent::IsItemAvailable(
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
