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

void UDRShopTransactionComponent::RequestSell(AActor* ShopActor, FGuid InstanceId)
{
	if (IsValid(ShopActor) && InstanceId.IsValid())
	{
		ServerRequestSell(ShopActor, InstanceId);
	}
}

void UDRShopTransactionComponent::RequestSellPerk(AActor* ShopActor, FGuid PerkInstanceId)
{
	if (IsValid(ShopActor) && PerkInstanceId.IsValid())
	{
		ServerRequestSellPerk(ShopActor, PerkInstanceId);
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
				ShopComponent,
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

void UDRShopTransactionComponent::ServerRequestSell_Implementation(
	AActor* ShopActor,
	FGuid InstanceId)
{
	ADRPlayerState* PlayerState = GetPlayerState();
	const UDRShopComponent* ShopComponent = IsValid(ShopActor)
		? ShopActor->FindComponentByClass<UDRShopComponent>()
		: nullptr;
	UDRInventoryComponent* Inventory = GetInventoryComponent();
	const FDRItemInstance* ItemInstance = IsValid(Inventory)
		? Inventory->FindItemInstance(InstanceId)
		: nullptr;
	const UDRItemDefinition* Definition = ItemInstance
		? ItemInstance->Definition
		: nullptr;

	// 클라이언트가 보낸 가격은 사용하지 않고 서버의 ItemDefinition만 신뢰한다.
	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(Inventory)
		|| !IsValid(Definition)
		|| !Definition->IsSellable()
		|| !ShopComponent->IsTransactionAllowed(PlayerState->GetPawn()))
	{
		return;
	}

	const int32 SellPrice = Definition->GetSellPrice();

	if (Inventory->TryRemoveItemInstance(InstanceId, 1))
	{
		PlayerState->AddCoins(SellPrice);
	}
}

void UDRShopTransactionComponent::ServerRequestSellPerk_Implementation(
	AActor* ShopActor,
	FGuid PerkInstanceId)
{
	ADRPlayerState* PlayerState = GetPlayerState();
	const UDRShopComponent* ShopComponent = IsValid(ShopActor)
		? ShopActor->FindComponentByClass<UDRShopComponent>()
		: nullptr;
	UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;
	const UDRPerkDefinition* PerkDefinition = IsValid(PerkComponent)
		? PerkComponent->FindPerkDefinition(PerkInstanceId)
		: nullptr;

	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(PerkComponent)
		|| !IsValid(PerkDefinition)
		|| !PerkDefinition->IsSellable()
		|| !ShopComponent->IsTransactionAllowed(PlayerState->GetPawn()))
	{
		return;
	}

	const int32 SellPrice = PerkDefinition->GetSellPrice();
	if (PerkComponent->TryRemovePerk(PerkInstanceId))
	{
		PlayerState->AddCoins(SellPrice);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Perk][Sold] Player=%s PerkId=%s Perk=%s Price=%d"),
			*GetNameSafe(PlayerState),
			*PerkInstanceId.ToString(),
			*GetNameSafe(PerkDefinition),
			SellPrice);
	}
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
		|| !ShopComponent->CanPurchaseItem(
			Inventory,
			ItemDefinition,
			PlayerState->GetCoins())
		|| !Inventory->TryAddItem(ItemDefinition, 1))
	{
		return false;
	}

	PlayerState->SetCoins(PlayerState->GetCoins() - ItemDefinition->Price);
	return true;
}
bool UDRShopTransactionComponent::TryUpgrade(
	ADRPlayerState* PlayerState,
	const UDRShopComponent* ShopComponent,
	const UDRUpgradeComponent* UpgradeComponent,
	UDRInventoryComponent* Inventory,
	const FDRShopItemTableRow& ItemRow,
	int32 TargetLevel) const
{
	FDRUpgradeOperation Operation;

	// 현재 인벤토리를 기준으로 작업을 다시 만들고 성공한 경우에만 비용을 차감한다.
	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(UpgradeComponent)
		|| !IsValid(Inventory)
		|| !UpgradeComponent->BuildUpgradeOperation(
			ItemRow,
			TargetLevel,
			Inventory,
			Operation)
		|| !IsValid(Operation.TargetDefinition)
		|| !ShopComponent->CanAfford(
			Operation.TargetDefinition,
			PlayerState->GetCoins())
		|| !UpgradeComponent->ApplyUpgrade(Inventory, Operation))
	{
		return false;
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

	// 클라이언트 요청을 신뢰하지 않고 가격, 슬롯과 Row 데이터를 서버에서 재검증한다.
	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(PerkComponent)
		|| !ShopComponent->GetPerkDefinition(RowName, PerkDefinition)
		|| !ShopComponent->CanPurchasePerk(
			PerkDefinition,
			PerkComponent,
			PlayerState->GetCoins()))
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

	// 검증된 퍽의 AbilitySet 적용이 성공한 경우에만 구매를 확정한다.
	if (!IsValid(PerkDefinition)
		|| !PerkComponent->AddPerk(PerkDefinition))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][PurchaseFailed] Player=%s Row=%s Perk=%s Reason=AddFailed"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	// 퍽 적용이 완료된 뒤 비용을 차감한다.
	const int32 PreviousCoins = PlayerState->GetCoins();
	PlayerState->SetCoins(PreviousCoins - PerkDefinition->Price);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][PurchaseSucceeded] Player=%s Row=%s Perk=%s Count=%d Price=%d Coins=%d->%d"),
		*GetNameSafe(PlayerState),
		*RowName.ToString(),
		*GetNameSafe(PerkDefinition),
		PerkComponent->GetPerkCount(PerkDefinition),
		PerkDefinition->Price,
		PreviousCoins,
		PlayerState->GetCoins());
	return true;
}
