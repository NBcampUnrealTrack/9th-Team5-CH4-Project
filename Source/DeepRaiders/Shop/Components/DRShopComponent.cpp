#include "DRShopComponent.h"

#include "DRShopAreaComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"

UDRShopComponent::UDRShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	// 서버가 RowName으로 원본 상점 데이터를 다시 조회한다.
	if (!GetItemRow(RowName, ItemRow))
	{
		return false;
	}

	// 일반 아이템 Row가 퍽 구매 경로로 들어오는 것을 차단한다.
	OutPerkDefinition = Cast<UDRPerkDefinition>(ItemRow.ItemDefinition);
	return IsValid(OutPerkDefinition);
}

bool UDRShopComponent::CanPurchasePerk(
	const UDRPerkDefinition* PerkDefinition,
	const UDRPerkComponent* PerkComponent,
	float AvailableSnowGauge) const
{
	return IsValid(PerkComponent)
		&& IsValid(PerkDefinition)
		&& PerkComponent->CanAddPerkAutomatically(PerkDefinition)
		&& CanAfford(PerkDefinition, AvailableSnowGauge);
}

bool UDRShopComponent::CanAfford(
	const UDRItemDefinition* ItemDefinition,
	float AvailableSnowGauge) const
{
	return IsValid(ItemDefinition)
		&& ItemDefinition->Price >= 0
		&& AvailableSnowGauge >= ItemDefinition->Price;
}

bool UDRShopComponent::CanPurchaseItem(
	const UDRInventoryComponent* Inventory,
	UDRItemDefinition* ItemDefinition,
	float AvailableSnowGauge) const
{
	return IsValid(Inventory)
		&& IsValid(ItemDefinition)
		&& ItemOffers.ContainsByPredicate(
			[ItemDefinition](const FDRShopItemOffer& ItemOffer)
			{
				return ItemOffer.OfferType == EDRShopOfferType::Purchase
					&& ItemOffer.ItemDefinition == ItemDefinition;
			})
		&& CanAfford(ItemDefinition, AvailableSnowGauge)
		&& Inventory->CanAddItem(ItemDefinition, 1);
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
	// PerkDefinition은 퍽 Offer로 등록한다.
	if (IsValid(Cast<UDRPerkDefinition>(ItemRow.ItemDefinition)))
	{
		AddOffer(RowName, EDRShopOfferType::Perk, ItemRow.ItemDefinition);
		return;
	}

	if (ItemRow.ItemDefinition->Category != EDRItemCategory::Equipment
		&& ItemRow.ItemDefinition->Category != EDRItemCategory::Consumable)
	{
		return;
	}

	AddOffer(RowName, EDRShopOfferType::Purchase, ItemRow.ItemDefinition);
}

void UDRShopComponent::AddOffer(
	FName RowName,
	EDRShopOfferType OfferType,
	UDRItemDefinition* ItemDefinition)
{
	FDRShopItemOffer& Offer = ItemOffers.AddDefaulted_GetRef();
	Offer.RowName = RowName;
	Offer.OfferType = OfferType;
	Offer.ItemDefinition = ItemDefinition;
}
