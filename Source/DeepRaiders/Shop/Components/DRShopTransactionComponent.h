#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopTransactionComponent.generated.h"

class ADRPlayerState;
class UDRInventoryComponent;
class UDRPerkComponent;
class UDRShopComponent;
class UDRUpgradeComponent;
class USoundBase;

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRShopTransactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopTransactionComponent();

	/** 로컬 Offer 요청을 서버 권한 거래로 전달한다. */
	void RequestOffer(
		AActor* ShopActor,
		const FDRShopOfferRequest& Request);

	/** 판매 가능한 광물 전체의 서버 판매를 요청한다. */
	void RequestSellAllOres(AActor* ShopActor);

protected:
	/** 상점 접근과 Row 데이터를 재검증한 뒤 구매 또는 업그레이드를 실행한다. */
	UFUNCTION(Server, Reliable)
	void ServerRequestOffer(
		AActor* ShopActor,
		FDRShopOfferRequest Request);

	/** 판매 대상을 서버에서 다시 계산한 뒤 인벤토리와 코인을 갱신한다. */
	UFUNCTION(Server, Reliable)
	void ServerSellAllOres(AActor* ShopActor);

	UFUNCTION(Client, Reliable)
	void ClientPlayTransactionSound(
		USoundBase* Sound,
		float VolumeMultiplier);

private:
	/** 이 컴포넌트를 소유한 Controller의 PlayerState를 반환한다. */
	ADRPlayerState* GetPlayerState() const;

	/** 이 컴포넌트를 소유한 Controller의 인벤토리를 반환한다. */
	UDRInventoryComponent* GetInventoryComponent() const;

	void PlayPurchaseSound(const AActor* ShopActor);
	void PlaySellSound(const AActor* ShopActor);

	/** 일반 상품의 가격과 인벤토리 공간을 검증하고 구매를 확정한다. */
	bool TryPurchase(
		ADRPlayerState* PlayerState,
		const UDRShopComponent* ShopComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow) const;

	/** 업그레이드 작업을 생성·적용하고 비용을 차감한다. */
	bool TryUpgrade(
		ADRPlayerState* PlayerState,
		const UDRUpgradeComponent* UpgradeComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow,
		int32 TargetLevel) const;

	bool TryPurchasePerk(
		ADRPlayerState* PlayerState,
		const UDRShopComponent* ShopComponent,
		UDRPerkComponent* PerkComponent,
		FName RowName) const;

	/** 판매 가능한 광물 Entry와 총 판매 금액을 안전하게 계산한다. */
	int64 CollectSellableOreEntries(
		const UDRInventoryComponent* Inventory,
		TArray<FGuid>& OutEntryIds,
		int32& OutTotalQuantity) const;
};
