#include "DRShopTestPlayerState.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"
#include "Net/UnrealNetwork.h"

void ADRShopTestPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRShopTestPlayerState, Coins);
}

int32 ADRShopTestPlayerState::GetCoins() const
{
	return Coins;
}

void ADRShopTestPlayerState::SetCoins(int32 NewCoins)
{
	// 코인 값은 서버에서만 변경합니다.
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

void ADRShopTestPlayerState::RequestPurchase(
	AActor* ShopActor,
	UDRItemDefinition* ItemDefinition)
{
	if (IsValid(ShopActor) && IsValid(ItemDefinition))
	{
		ServerPurchase(ShopActor, ItemDefinition);
	}
}

void ADRShopTestPlayerState::OnRep_Coins(int32 PreviousCoins)
{
	// 서버와 클라이언트가 동일한 이벤트로 UI를 갱신합니다.
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Coins changed: Previous=%d New=%d"),
		PreviousCoins,
		Coins);

	OnCoinsChanged.Broadcast(Coins);
}

bool ADRShopTestPlayerState::ServerPurchase_Validate(
	AActor* ShopActor,
	UDRItemDefinition* ItemDefinition)
{
	if (!IsValid(ShopActor) || !IsValid(ItemDefinition))
	{
		return false;
	}

	const UDRShopUIComponent* ShopUIComponent =
		ShopActor->FindComponentByClass<UDRShopUIComponent>();

	// 요청한 아이템이 해당 상점의 판매 목록에 있는지 확인합니다.
	return IsValid(ShopUIComponent)
		&& ShopUIComponent->IsItemAvailable(ItemDefinition);
}

void ADRShopTestPlayerState::ServerPurchase_Implementation(
	AActor* ShopActor,
	UDRItemDefinition* ItemDefinition)
{
	if (!IsValid(ShopActor) || !IsValid(ItemDefinition)
		|| ItemDefinition->Price <= 0)
	{
		return;
	}

	const UDRShopUIComponent* ShopUIComponent =
		ShopActor->FindComponentByClass<UDRShopUIComponent>();

	// 상호작용 범위와 보유 코인을 서버에서 최종 검증합니다.
	if (!IsValid(ShopUIComponent)
		|| !ShopUIComponent->CanPurchase(GetPawn(), ItemDefinition)
		|| Coins < ItemDefinition->Price)
	{
		return;
	}

	SetCoins(Coins - ItemDefinition->Price);
}
