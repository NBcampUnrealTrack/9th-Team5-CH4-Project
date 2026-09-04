#include "DRShopTransactionComponent.h"

#include "DRShopComponent.h"
#include "DeepRaiders/Upgrade/DRCharacterUpgradeComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/Upgrade/DRWeaponUpgradeProfile.h"
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
	if (!IsValid(ShopActor) || !Request.IsValidRequest())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=Request Reason=%s Owner=%s Type=%d Row=%s Tag=%s"),
			!IsValid(ShopActor) ? TEXT("InvalidShop") : TEXT("InvalidRequest"),
			*GetNameSafe(GetOwner()), static_cast<int32>(Request.OfferType),
			*Request.RowName.ToString(), *Request.UpgradeTag.ToString());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Shop][Request] Owner=%s Shop=%s Type=%d Row=%s Tag=%s ExpectedLevel=%d"),
		*GetNameSafe(GetOwner()), *GetNameSafe(ShopActor), static_cast<int32>(Request.OfferType),
		*Request.RowName.ToString(), *Request.UpgradeTag.ToString(), Request.ExpectedLevel);
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

	UE_LOG(LogTemp, Log, TEXT("[Shop][ServerRequest] Player=%s Shop=%s Type=%d Row=%s Tag=%s ExpectedLevel=%d"),
		*GetNameSafe(PlayerState), *GetNameSafe(ShopActor), static_cast<int32>(Request.OfferType),
		*Request.RowName.ToString(), *Request.UpgradeTag.ToString(), Request.ExpectedLevel);

	const TCHAR* FailureReason = !IsValid(PlayerState) ? TEXT("MissingPlayerState")
		: !IsValid(ShopComponent) ? TEXT("MissingShopComponent")
		: !Request.IsValidRequest() ? TEXT("InvalidRequest")
		: !ShopComponent->IsTransactionAllowed(PlayerState->GetPawn()) ? TEXT("OutsideShopArea")
		: (Request.OfferType == EDRShopOfferType::Purchase && !IsValid(Inventory)) ? TEXT("MissingInventory")
		: nullptr;
	if (FailureReason)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=ServerValidation Reason=%s Player=%s Shop=%s Tag=%s"),
			FailureReason, *GetNameSafe(PlayerState), *GetNameSafe(ShopActor), *Request.UpgradeTag.ToString());
		return;
	}

	switch (Request.OfferType)
	{
	case EDRShopOfferType::WeaponUpgrade:
		if (TryPurchaseWeaponUpgrade(PlayerState, Inventory, Request))
		{
			PlayPurchaseSound(ShopActor);
		}
		break;

	case EDRShopOfferType::CharacterUpgrade:
		if (TryPurchaseCharacterUpgrade(PlayerState, Request))
		{
			PlayPurchaseSound(ShopActor);
		}
		break;

	case EDRShopOfferType::Purchase:
		if (ShopComponent->GetItemRow(Request.RowName, ItemRow)
			&& TryPurchase(PlayerState, ShopComponent, Inventory, ItemRow))
		{
			UE_LOG(LogTemp, Log, TEXT("[Shop][PurchaseSucceeded] Type=Item Player=%s Row=%s Price=%d Currency=%.2f"),
				*GetNameSafe(PlayerState), *Request.RowName.ToString(), ItemRow.ItemDefinition->Price, PlayerState->GetSnowGauge());
			PlayPurchaseSound(ShopActor);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=ItemPurchase Reason=RowOrPurchaseRejected Player=%s Row=%s Currency=%.2f"),
				*GetNameSafe(PlayerState), *Request.RowName.ToString(), PlayerState->GetSnowGauge());
		}
		break;

	case EDRShopOfferType::Perk:
		if (TryPurchasePerk(
				PlayerState,
				ShopComponent,
				PlayerState->GetPerkComponent(),
				Request.RowName))
		{
			UE_LOG(LogTemp, Log, TEXT("[Shop][PurchaseSucceeded] Type=Perk Player=%s Row=%s Currency=%.2f"),
				*GetNameSafe(PlayerState), *Request.RowName.ToString(), PlayerState->GetSnowGauge());
			PlayPurchaseSound(ShopActor);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=PerkPurchase Reason=PerkPurchaseRejected Player=%s Row=%s"),
				*GetNameSafe(PlayerState), *Request.RowName.ToString());
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
		PlayerState->AddSnowGauge(SellPrice);
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
		PlayerState->AddSnowGauge(SellPrice);
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

	// 가격, 판매 목록, 눈, 인벤토리 공간을 모두 검증한 뒤 아이템을 추가한다.
	if (!IsValid(PlayerState)
		|| !IsValid(ShopComponent)
		|| !IsValid(Inventory)
		|| !IsValid(ItemDefinition)
		|| !ShopComponent->CanPurchaseItem(
			Inventory,
			ItemDefinition,
			PlayerState->GetSnowGauge())
		|| !Inventory->TryAddItem(ItemDefinition, 1))
	{
		return false;
	}

	PlayerState->AddSnowGauge(-ItemDefinition->Price);
	return true;
}

bool UDRShopTransactionComponent::TryPurchaseCharacterUpgrade(
	ADRPlayerState* PlayerState, const FDRShopOfferRequest& Request) const
{
	UDRCharacterUpgradeComponent* UpgradeComponent = PlayerState->GetCharacterUpgradeComponent();
	const FDRStatUpgradeData* UpgradeData = IsValid(UpgradeComponent)
		? UpgradeComponent->GetUpgradeData(Request.UpgradeTag) : nullptr;
	const float PreviousSnowGauge = PlayerState->GetSnowGauge();
	const TCHAR* UpgradeFailure = !IsValid(UpgradeComponent) ? TEXT("MissingUpgradeComponent")
		: !UpgradeData ? TEXT("InvalidProfileOrUpgradeTag")
		: PreviousSnowGauge < UpgradeData->Price ? TEXT("InsufficientCurrency")
		: nullptr;
	if (UpgradeFailure)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=UpgradeValidation Reason=%s Player=%s Tag=%s Price=%d Currency=%.2f"),
			UpgradeFailure, *GetNameSafe(PlayerState), *Request.UpgradeTag.ToString(),
			UpgradeData ? UpgradeData->Price : -1, PreviousSnowGauge);
		return false;
	}
	if (!UpgradeComponent->TryUpgrade(Request.UpgradeTag, Request.ExpectedLevel))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop][PurchaseFailed] Stage=UpgradeApply Reason=UpgradeRejected Player=%s Tag=%s ExpectedLevel=%d CurrentLevel=%d"),
			*GetNameSafe(PlayerState), *Request.UpgradeTag.ToString(), Request.ExpectedLevel,
			UpgradeComponent->GetUpgradeLevel(Request.UpgradeTag));
		return false;
	}
	PlayerState->AddSnowGauge(-UpgradeData->Price);
	UE_LOG(LogTemp, Log, TEXT("[Shop][PurchaseSucceeded] Type=CharacterUpgrade Player=%s Tag=%s Level=%d->%d Price=%d Currency=%.2f->%.2f"),
		*GetNameSafe(PlayerState), *Request.UpgradeTag.ToString(), Request.ExpectedLevel,
		UpgradeComponent->GetUpgradeLevel(Request.UpgradeTag), UpgradeData->Price,
		PreviousSnowGauge, PlayerState->GetSnowGauge());
	return true;
}

bool UDRShopTransactionComponent::TryPurchaseWeaponUpgrade(
	ADRPlayerState* PlayerState, UDRInventoryComponent* Inventory, const FDRShopOfferRequest& Request) const
{
	if (!IsValid(PlayerState) || !IsValid(Inventory) || !Request.IsValidRequest())
	{
		return false;
	}

	const FDRItemInstance* Item = Inventory->FindItemInstance(Request.InstanceId);
	const UDRProjectileWeaponItemDefinition* Weapon = Item != nullptr
		? Cast<UDRProjectileWeaponItemDefinition>(Item->Definition.Get()) : nullptr;
	if (!IsValid(Weapon) || Weapon->ResourceType != EDRProjectileWeaponResourceType::SnowGauge
		|| !IsValid(Weapon->UpgradeProfile) || !Weapon->UpgradeProfile->IsUsable())
	{
		return false;
	}

	const FDRWeaponUpgradeLevelData* NextLevel = Weapon->UpgradeProfile->FindLevelData(
		Request.UpgradeTag, Request.ExpectedLevel + 1);
	if (NextLevel == nullptr || PlayerState->GetSnowGauge() < NextLevel->Price)
	{
		return false;
	}

	const int32 Price = NextLevel->Price;
	if (!Inventory->TryUpgradeSnowProjectileWeapon(Request.InstanceId, Request.UpgradeTag, Request.ExpectedLevel))
	{
		return false;
	}

	PlayerState->AddSnowGauge(-Price);
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
			PlayerState->GetSnowGauge()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][PurchaseRejected] Player=%s Row=%s SnowGauge=%.2f Reason=PurchaseValidationFailed"),
			*GetNameSafe(PlayerState),
			*RowName.ToString(),
			IsValid(PlayerState) ? PlayerState->GetSnowGauge() : 0);
		return false;
	}

	// 검증된 퍽의 AbilitySet 적용이 성공한 경우에만 구매를 확정한다.
	if (!PerkComponent->AddPerkAutomatically(PerkDefinition))
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
	const float PreviousSnowGauge = PlayerState->GetSnowGauge();
	PlayerState->AddSnowGauge(-PerkDefinition->Price);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][PurchaseSucceeded] Player=%s Row=%s Perk=%s Count=%d Price=%d SnowGauge=%.2f->%.2f"),
		*GetNameSafe(PlayerState),
		*RowName.ToString(),
		*GetNameSafe(PerkDefinition),
		PerkComponent->GetPerkCount(PerkDefinition),
		PerkDefinition->Price,
		PreviousSnowGauge,
		PlayerState->GetSnowGauge());
	return true;
}

