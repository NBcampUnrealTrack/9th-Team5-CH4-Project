#include "DRPlayerState.h"

#include "DRPlayerCharacter.h"
#include "DRPlayerController.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"
#include "Net/UnrealNetwork.h"

void ADRPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRPlayerState, bHasDeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, DeepestDigLocation);

	// 제트팩 보유 여부는 다른 플레이어도 알아야 한다.
	DOREPLIFETIME(
		ADRPlayerState,
		bHasJetpack);

	// 연료는 해당 플레이어 자신에게만 보내도 된다.
	DOREPLIFETIME_CONDITION(
		ADRPlayerState,
		CurrentJetpackFuel,
		COND_OwnerOnly);

	DOREPLIFETIME(ADRPlayerState, Coins);
	DOREPLIFETIME(ADRPlayerState, TeamId);
}

bool ADRPlayerState::UpdateDeepestDigLocation(const FVector& Location)
{
	if (!HasAuthority() ||
		(bHasDeepestDigLocation && Location.Z >= DeepestDigLocation.Z))
	{
		return false;
	}

	bHasDeepestDigLocation = true;
	DeepestDigLocation = Location;
	ForceNetUpdate();
	return true;
}

void ADRPlayerState::GrantJetpack()
{
	if (!HasAuthority())
	{
		return;
	}

	bHasJetpack = true;
	CurrentJetpackFuel = MaxJetpackFuel;

	/*
	 * 서버에서는 RepNotify가 자동 호출되지 않으므로
	 * 리슨 서버 화면을 위해 직접 외형을 갱신한다.
	 */
	RefreshJetpackVisualOnPawn();

	// 일회성 획득 상태를 빠르게 전송하도록 요청한다.
	ForceNetUpdate();
}

bool ADRPlayerState::ConsumeJetpackFuel(float Amount)
{
	if (!HasAuthority() ||
		!bHasJetpack ||
		Amount <= 0.f ||
		CurrentJetpackFuel <= 0.f)
	{
		return false;
	}

	CurrentJetpackFuel = FMath::Max(
		0.f,
		CurrentJetpackFuel - Amount);

	// 이번 호출에서 연료 소비가 실행되었다는 의미
	return true;
}

bool ADRPlayerState::RefillJetpackFuel()
{
	if (!HasAuthority() || !bHasJetpack)
	{
		return false;
	}

	// 이미 가득 차 있으면 값을 다시 변경하지 않는다.
	if (FMath::IsNearlyEqual(
			CurrentJetpackFuel,
			MaxJetpackFuel))
	{
		return false;
	}

	CurrentJetpackFuel = MaxJetpackFuel;

	// 착지는 일회성 이벤트이므로 즉시 복제를 요청한다.
	ForceNetUpdate();

	return true;
}

int32 ADRPlayerState::GetCoins() const
{
	return Coins;
}

void ADRPlayerState::SetCoins(int32 NewCoins)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 PreviousCoins = Coins;
	const int32 ClampedCoins = FMath::Max(0, NewCoins);

	if (PreviousCoins == ClampedCoins)
	{
		return;
	}

	Coins = ClampedCoins;
	OnRep_Coins(PreviousCoins);
	ForceNetUpdate();
}

void ADRPlayerState::RequestPurchase(
	AActor* ShopActor,
	UDRItemDefinition* ItemDefinition)
{
	if (IsValid(ShopActor) && IsValid(ItemDefinition))
	{
		ServerPurchase(ShopActor, ItemDefinition);
	}
}

void ADRPlayerState::RequestSellAllOres(AActor* ShopActor)
{
	if (IsValid(ShopActor))
	{
		ServerSellAllOres(ShopActor);
	}
}

void ADRPlayerState::OnRep_Coins(int32 PreviousCoins)
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Coins changed: Previous=%d New=%d"),
		PreviousCoins,
		Coins);

	OnCoinsChanged.Broadcast(Coins);
}

bool ADRPlayerState::ServerPurchase_Validate(
	AActor* ShopActor,
	UDRItemDefinition* ItemDefinition)
{
	if (!IsValid(ShopActor) || !IsValid(ItemDefinition))
	{
		return false;
	}

	const UDRShopUIComponent* ShopUIComponent =
		ShopActor->FindComponentByClass<UDRShopUIComponent>();

	return IsValid(ShopUIComponent)
		&& ShopUIComponent->IsItemAvailable(ItemDefinition);
}

void ADRPlayerState::ServerPurchase_Implementation(
	AActor* ShopActor,
	UDRItemDefinition* ItemDefinition)
{
	// 서버에서 상점, 가격, 인벤토리 공간을 검증한다.
	if (!IsValid(ShopActor) || !IsValid(ItemDefinition)
		|| ItemDefinition->Price <= 0)
	{
		return;
	}

	const UDRShopUIComponent* ShopUIComponent =
		ShopActor->FindComponentByClass<UDRShopUIComponent>();
	UDRInventoryComponent* Inventory = GetInventoryComponent();

	if (!IsValid(ShopUIComponent)
		|| !ShopUIComponent->CanPurchase(GetPawn(), ItemDefinition)
		|| Coins < ItemDefinition->Price
		|| !IsValid(Inventory)
		|| !Inventory->CanAddItem(ItemDefinition, 1))
	{
		return;
	}

	// 아이템 추가가 완료된 경우에만 코인을 차감한다.
	if (!Inventory->TryAddItem(ItemDefinition, 1))
	{
		return;
	}

	SetCoins(Coins - ItemDefinition->Price);
}

void ADRPlayerState::ServerSellAllOres_Implementation(AActor* ShopActor)
{
	// 서버에서 상점 범위와 플레이어 인벤토리를 검증한다.
	if (!IsValid(ShopActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SellAllOres] Invalid ShopActor"));
		return;
	}

	const UDRShopUIComponent* ShopUIComponent =
		ShopActor->FindComponentByClass<UDRShopUIComponent>();
	UDRInventoryComponent* Inventory = GetInventoryComponent();
	const bool IsSellAllowed = IsValid(ShopUIComponent)
		&& ShopUIComponent->IsSellAllowed(GetPawn());

	if (!IsValid(ShopUIComponent)
		|| !IsSellAllowed
		|| !IsValid(Inventory))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SellAllOres] Validation failed. Shop=%s, IsShopUIValid=%d, IsSellAllowed=%d, IsInventoryValid=%d"),
			*GetNameSafe(ShopActor),
			IsValid(ShopUIComponent),
			IsSellAllowed,
			IsValid(Inventory));
		return;
	}

	TArray<FGuid> EntryIds;
	int32 TotalQuantity = 0;
	const int64 TotalPrice = CollectSellableOreEntries(
		Inventory,
		EntryIds,
		TotalQuantity);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SellAllOres] Summary. StackCount=%d, TotalQuantity=%d, TotalPrice=%lld"),
		EntryIds.Num(),
		TotalQuantity,
		TotalPrice);

	// 모든 판매 대상을 한 번에 제거한 뒤 총금액을 지급한다.
	if (EntryIds.IsEmpty()
		|| TotalPrice <= 0
		|| TotalPrice > static_cast<int64>(MAX_int32) - Coins
		|| !Inventory->TryRemoveEntries(EntryIds))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SellAllOres] Sale failed. CurrentCoins=%d, TotalPrice=%lld"),
			Coins,
			TotalPrice);
		return;
	}

	SetCoins(Coins + static_cast<int32>(TotalPrice));
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SellAllOres] Sale succeeded. SoldQuantity=%d, TotalPrice=%lld, CurrentCoins=%d"),
		TotalQuantity,
		TotalPrice,
		Coins);
}

UDRInventoryComponent* ADRPlayerState::GetInventoryComponent() const
{
	const ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(GetOwner());

	return IsValid(PlayerController)
		? PlayerController->GetQuickSlotInventoryComponent()
		: nullptr;
}

int64 ADRPlayerState::CollectSellableOreEntries(
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
			|| Definition->Category != EItemCategory::Ore)
		{
			continue;
		}

		if (!Definition->bCanBeSold
			|| Definition->Price <= 0)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[SellAllOres] Ore excluded. ItemId=%s, Quantity=%d, IsSellable=%d, Price=%d"),
				*Definition->ItemId.ToString(),
				Entry.Quantity,
				Definition->bCanBeSold,
				Definition->Price);
			continue;
		}

		const int64 Subtotal =
			static_cast<int64>(Definition->Price) * Entry.Quantity;
		OutEntryIds.Add(Entry.EntryId);
		OutTotalQuantity += Entry.Quantity;
		TotalPrice += Subtotal;

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[SellAllOres] Ore found. ItemId=%s, Quantity=%d, UnitPrice=%d, Subtotal=%lld"),
			*Definition->ItemId.ToString(),
			Entry.Quantity,
			Definition->Price,
			Subtotal);
	}

	return TotalPrice;
}

void ADRPlayerState::OnRep_HasJetpack()
{
	RefreshJetpackVisualOnPawn();
}

void ADRPlayerState::OnRep_JetpackFuel()
{
	/*
	 * 추후 HUD 연료 게이지 갱신용.
	 *
	 * 현재 UI가 없다면 비어 있어도 동작에는 문제가 없지만,
	 * 아무 처리도 계속 하지 않을 거면 ReplicatedUsing 대신
	 * Replicated로 바꿔도 된다.
	 */
}

void ADRPlayerState::RefreshJetpackVisualOnPawn()
{
	ADRPlayerCharacter* PlayerCharacter =
		GetPawn<ADRPlayerCharacter>();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RefreshJetpackVisual();
}

#pragma region Teleport
void ADRPlayerState::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority() || TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
	ForceNetUpdate();
}
#pragma endregion
