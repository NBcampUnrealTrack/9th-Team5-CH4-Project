#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopTransactionComponent.generated.h"

class ADRPlayerState;
class UDRInventoryComponent;
class UDRShopComponent;
class UDRUpgradeComponent;

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRShopTransactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopTransactionComponent();

	void RequestOffer(
		AActor* ShopActor,
		const FDRShopOfferRequest& Request);
	void RequestSellAllOres(AActor* ShopActor);

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestOffer(
		AActor* ShopActor,
		FDRShopOfferRequest Request);

	UFUNCTION(Server, Reliable)
	void ServerSellAllOres(AActor* ShopActor);

private:
	ADRPlayerState* GetPlayerState() const;
	UDRInventoryComponent* GetInventoryComponent() const;

	bool TryPurchase(
		ADRPlayerState* PlayerState,
		const UDRShopComponent* ShopComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow) const;

	bool TryUpgrade(
		ADRPlayerState* PlayerState,
		const UDRUpgradeComponent* UpgradeComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow,
		int32 TargetLevel) const;

	int64 CollectSellableOreEntries(
		const UDRInventoryComponent* Inventory,
		TArray<FGuid>& OutEntryIds,
		int32& OutTotalQuantity) const;
};
