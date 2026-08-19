#include "DRShopTransactionComponent.h"

#include "DRShopComponent.h"
#include "DRUpgradeComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Shop/DRShop.h"
#include "Kismet/GameplayStatics.h"

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

	// 클라이언트 요청을 신뢰하지 않고 상점 접근 상태와 Row를 서버에서 다시 확인한다.
	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(Inventory)
		|| Request.RowName.IsNone()
		|| !ShopComponent->IsTransactionAllowed(PlayerState->GetPawn()))
	{
		return;
	}

	switch (Request.OfferType)
	{
	case EDRShopOfferType::Purchase:
		if (ShopComponent->GetItemRow(Request.RowName, ItemRow)
			&& TryPurchase(PlayerState, ShopComponent, Inventory, ItemRow))
		{
			PlayPurchaseSound(ShopActor);
		}
		break;

	case EDRShopOfferType::Upgrade:
		if (ShopComponent->GetItemRow(Request.RowName, ItemRow)
			&& TryUpgrade(
			PlayerState,
			ShopActor->FindComponentByClass<UDRUpgradeComponent>(),
			Inventory,
			ItemRow,
			Request.TargetLevel))
		{
			PlayPurchaseSound(ShopActor);
		}
		break;

	case EDRShopOfferType::Perk:
		if (TryPurchasePerk(
				PlayerState,
				ShopComponent,
				PlayerState->GetPerkComponent(),
				Request.RowName))
		{
			PlayPurchaseSound(ShopActor);
		}
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

	// 인벤토리 제거가 완료된 경우에만 판매 금액을 지급한다.
	if (EntryIds.IsEmpty()
		|| TotalPrice <= 0
		|| TotalPrice > static_cast<int64>(MAX_int32) - PlayerState->GetCoins()
		|| !Inventory->TryRemoveEntries(EntryIds))
	{
		return;
	}

	PlayerState->SetCoins(
		PlayerState->GetCoins() + static_cast<int32>(TotalPrice));
	PlaySellSound(ShopActor);
}

void UDRShopTransactionComponent::ClientPlayTransactionSound_Implementation(
	USoundBase* Sound,
	float VolumeMultiplier)
{
	if (IsValid(Sound) && VolumeMultiplier > 0.f)
	{
		UGameplayStatics::PlaySound2D(this, Sound, VolumeMultiplier);
	}
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

void UDRShopTransactionComponent::PlayPurchaseSound(
	const AActor* ShopActor)
{
	const ADRShop* Shop = Cast<ADRShop>(ShopActor);

	if (IsValid(Shop))
	{
		ClientPlayTransactionSound(
			Shop->GetPurchaseSound(),
			Shop->GetTransactionSoundVolume());
	}
}

void UDRShopTransactionComponent::PlaySellSound(
	const AActor* ShopActor)
{
	const ADRShop* Shop = Cast<ADRShop>(ShopActor);

	if (IsValid(Shop))
	{
		ClientPlayTransactionSound(
			Shop->GetSellSound(),
			Shop->GetTransactionSoundVolume());
	}
}

bool UDRShopTransactionComponent::TryPurchase(
	ADRPlayerState* PlayerState,
	const UDRShopComponent* ShopComponent,
	UDRInventoryComponent* Inventory,
	const FDRShopItemTableRow& ItemRow) const
{
	UDRItemDefinition* ItemDefinition = ItemRow.ItemDefinition;

	// 가격, 판매 목록, 코인, 인벤토리 공간을 모두 검증한 뒤 아이템을 추가한다.
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

	// 현재 인벤토리를 기준으로 작업을 다시 만들고 성공한 경우에만 비용을 차감한다.
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

	if (Operation.TargetLevel == 1)
	{
		ADRPlayerController* PlayerController =
			Cast<ADRPlayerController>(GetOwner());
		UDRQuickSlotComponent* QuickSlotComponent =
			IsValid(PlayerController)
				? PlayerController->GetQuickSlotComponent()
				: nullptr;
		bool IsBindingRestored = false;

		if (IsValid(QuickSlotComponent))
		{
			for (int32 Level = 2;
				Level <= ItemRow.GetMaxUpgradeLevel();
				++Level)
			{
				if (QuickSlotComponent->ReplaceBoundDefinition(
					ItemRow.GetDefinitionForLevel(Level),
					Operation.TargetDefinition))
				{
					IsBindingRestored = true;
					break;
				}
			}

			if (!IsBindingRestored)
			{
				QuickSlotComponent->TryBindFirstEmptySlot(
					Operation.TargetDefinition);
			}
		}
	}

	PlayerState->SetCoins(
		PlayerState->GetCoins() - Operation.TargetDefinition->Price);
	return true;
}

bool UDRShopTransactionComponent::TryPurchasePerk(
	ADRPlayerState* PlayerState,
	const UDRShopComponent* ShopComponent,
	UDRPerkComponent* PerkComponent,
	FName RowName) const
{
	UDRPerkDefinition* PerkDefinition = nullptr;

	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(PerkComponent)
		|| !ShopComponent->CanPurchasePerk(
			RowName,
			PerkComponent,
			PlayerState->GetCoins())
		|| !ShopComponent->GetPerkDefinition(RowName, PerkDefinition))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][PurchaseRejected] Player=%s Row=%s Coins=%d Reason=PurchaseValidationFailed"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			IsValid(PlayerState) ? PlayerState->GetCoins() : 0);
		return false;
	}

	if (!IsValid(PerkDefinition)
		|| !PerkComponent->AddTestPerk(PerkDefinition))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][Test][PurchaseFailed] Player=%s Row=%s Perk=%s Reason=AddFailed"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	const int32 PreviousCoins = PlayerState->GetCoins();
	PlayerState->SetCoins(PreviousCoins - PerkDefinition->Price);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][Test][PurchaseSucceeded] Player=%s Row=%s Perk=%s Count=%d Price=%d Coins=%d->%d"),
		*GetNameSafe(PlayerState),
		*RowName.ToString(),
		*GetNameSafe(PerkDefinition),
		PerkComponent->GetTestPerkCount(PerkDefinition),
		PerkDefinition->Price,
		PreviousCoins,
		PlayerState->GetCoins());
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
			|| Definition->Category != EDRItemCategory::Ore
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
