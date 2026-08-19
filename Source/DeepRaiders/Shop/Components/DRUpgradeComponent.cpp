#include "DRUpgradeComponent.h"

#include "DRShopComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

UDRUpgradeComponent::UDRUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<FDRShopItemOffer> UDRUpgradeComponent::GetNextUpgradeOffers(
	const UDRShopComponent* ShopComponent,
	const UDRInventoryComponent* Inventory) const
{
	TArray<FDRShopItemOffer> UpgradeOffers;

	if (!IsValid(ShopComponent) || !IsValid(Inventory))
	{
		return UpgradeOffers;
	}

	TSet<FName> ProcessedRows;

	// 동일 체인은 한 번만 처리하고 현재 보유 단계의 다음 Offer만 노출한다.
	for (const FDRShopItemOffer& ItemOffer : ShopComponent->GetItemOffers())
	{
		if (!ItemOffer.IsUpgrade()
			|| ProcessedRows.Contains(ItemOffer.RowName))
		{
			continue;
		}

		ProcessedRows.Add(ItemOffer.RowName);
		const int32 TargetLevel = GetOwnedUpgradeLevel(
			ItemOffer.RowName,
			ShopComponent,
			Inventory) + 1;
		const FDRShopItemOffer* NextOffer = FindUpgradeOffer(
			ItemOffer.RowName,
			TargetLevel,
			ShopComponent);
		FDRShopItemTableRow ItemRow;
		FDRUpgradeOperation Operation;

		if (NextOffer
			&& ShopComponent->GetItemRow(ItemOffer.RowName, ItemRow)
			&& BuildUpgradeOperation(
				ItemRow,
				TargetLevel,
				Inventory,
				Operation))
		{
			UpgradeOffers.Add(*NextOffer);
		}
	}

	return UpgradeOffers;
}

bool UDRUpgradeComponent::BuildUpgradeOperation(
	const FDRShopItemTableRow& ItemRow,
	int32 TargetLevel,
	const UDRInventoryComponent* Inventory,
	FDRUpgradeOperation& OutOperation) const
{
	OutOperation = FDRUpgradeOperation();

	if (!IsValid(Inventory)
		|| !ItemRow.IsValidUpgradeLevel(TargetLevel))
	{
		return false;
	}

	UDRItemDefinition* SourceDefinition =
		ItemRow.GetUpgradeSourceDefinition(TargetLevel);
	UDRItemDefinition* TargetDefinition =
		ItemRow.GetUpgradeTargetDefinition(TargetLevel);

	// 1단계는 신규 지급, 이후 단계는 직전 Definition을 가진 Entry 교체로 처리한다.
	if (!IsValid(TargetDefinition)
		|| TargetDefinition->Category != EDRItemCategory::Equipment)
	{
		return false;
	}

	if (TargetLevel == 1)
	{
		if (HasAnyItemInUpgradeChain(ItemRow, Inventory))
		{
			return false;
		}
	}
	else if (!IsValid(SourceDefinition)
		|| SourceDefinition == TargetDefinition
		|| SourceDefinition->Category != EDRItemCategory::Equipment
		|| !FindUpgradeSourceEntryId(
			Inventory,
			SourceDefinition,
			OutOperation.SourceEntryId))
	{
		return false;
	}

	OutOperation.SourceDefinition = SourceDefinition;
	OutOperation.TargetDefinition = TargetDefinition;
	OutOperation.TargetLevel = TargetLevel;
	return true;
}

bool UDRUpgradeComponent::ApplyUpgrade(
	UDRInventoryComponent* Inventory,
	const FDRUpgradeOperation& Operation) const
{
	if (!IsValid(Inventory)
		|| !IsValid(Operation.TargetDefinition)
		|| Operation.TargetLevel <= 0)
	{
		return false;
	}

	if (Operation.TargetLevel == 1)
	{
		return Inventory->TryAddItem(Operation.TargetDefinition, 1);
	}

	return Inventory->TryReplaceEntryDefinition(
		Operation.SourceEntryId,
		Operation.SourceDefinition,
		Operation.TargetDefinition);
}

int32 UDRUpgradeComponent::GetOwnedUpgradeLevel(
	FName RowName,
	const UDRShopComponent* ShopComponent,
	const UDRInventoryComponent* Inventory) const
{
	int32 OwnedLevel = 0;

	if (!IsValid(ShopComponent) || !IsValid(Inventory))
	{
		return OwnedLevel;
	}

	for (const FDRShopItemOffer& ItemOffer : ShopComponent->GetItemOffers())
	{
		if (ItemOffer.IsUpgrade()
			&& ItemOffer.RowName == RowName
			&& Inventory->GetItemCount(ItemOffer.ItemDefinition) > 0)
		{
			OwnedLevel = FMath::Max(OwnedLevel, ItemOffer.TargetLevel);
		}
	}

	return OwnedLevel;
}

const FDRShopItemOffer* UDRUpgradeComponent::FindUpgradeOffer(
	FName RowName,
	int32 TargetLevel,
	const UDRShopComponent* ShopComponent) const
{
	if (!IsValid(ShopComponent))
	{
		return nullptr;
	}

	return ShopComponent->GetItemOffers().FindByPredicate(
		[RowName, TargetLevel](const FDRShopItemOffer& ItemOffer)
		{
			return ItemOffer.IsUpgrade()
				&& ItemOffer.RowName == RowName
				&& ItemOffer.TargetLevel == TargetLevel;
		});
}

bool UDRUpgradeComponent::HasAnyItemInUpgradeChain(
	const FDRShopItemTableRow& ItemRow,
	const UDRInventoryComponent* Inventory) const
{
	if (!IsValid(Inventory))
	{
		return false;
	}

	for (int32 Level = 1; Level <= ItemRow.GetMaxUpgradeLevel(); ++Level)
	{
		if (Inventory->GetItemCount(ItemRow.GetDefinitionForLevel(Level)) > 0)
		{
			return true;
		}
	}

	return false;
}

bool UDRUpgradeComponent::FindUpgradeSourceEntryId(
	const UDRInventoryComponent* Inventory,
	const UDRItemDefinition* SourceDefinition,
	FGuid& OutEntryId) const
{
	OutEntryId.Invalidate();

	if (!IsValid(Inventory) || !IsValid(SourceDefinition))
	{
		return false;
	}

	for (const FDRInventoryEntry& Entry : Inventory->GetEntries())
	{
		if (Entry.IsValid()
			&& Entry.Quantity == 1
			&& Entry.Definition == SourceDefinition)
		{
			OutEntryId = Entry.EntryId;
			return true;
		}
	}

	return false;
}
