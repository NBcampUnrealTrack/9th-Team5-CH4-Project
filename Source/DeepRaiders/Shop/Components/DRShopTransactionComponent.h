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
	/** 서버 권한 상점 거래를 처리하는 복제 컴포넌트를 초기화한다. */
	UDRShopTransactionComponent();

	/** 로컬 Offer 요청을 서버 권한 거래로 전달한다. */
	void RequestOffer(
		AActor* ShopActor,
		const FDRShopOfferRequest& Request);

protected:
	/** 상점 접근과 Row 데이터를 재검증한 뒤 구매 또는 업그레이드를 실행한다. */
	UFUNCTION(Server, Reliable)
	void ServerRequestOffer(
		AActor* ShopActor,
		FDRShopOfferRequest Request);

	/** 거래 결과 사운드를 요청한 클라이언트에서 재생한다. */
	UFUNCTION(Client, Reliable)
	void ClientPlayTransactionSound(
		USoundBase* Sound,
		float VolumeMultiplier);

private:
	/** 이 컴포넌트를 소유한 Controller의 PlayerState를 반환한다. */
	ADRPlayerState* GetPlayerState() const;

	/** 이 컴포넌트를 소유한 Controller의 인벤토리를 반환한다. */
	UDRInventoryComponent* GetInventoryComponent() const;

	/** 상점에 설정된 구매 사운드를 요청한 클라이언트에 전달한다. */
	void PlayPurchaseSound(const AActor* ShopActor);

	/** 일반 상품의 가격과 인벤토리 공간을 검증하고 구매를 확정한다. */
	bool TryPurchase(
		ADRPlayerState* PlayerState,
		const UDRShopComponent* ShopComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow) const;

	/** 업그레이드 작업을 생성·적용하고 비용을 차감한다. */
	bool TryUpgrade(
		ADRPlayerState* PlayerState,
		const UDRShopComponent* ShopComponent,
		const UDRUpgradeComponent* UpgradeComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow,
		int32 TargetLevel) const;

	/** 공용 구매 검증 후 퍽 효과를 적용하고 비용을 차감한다. */
	bool TryPurchasePerk(
		ADRPlayerState* PlayerState,
		const UDRShopComponent* ShopComponent,
		UDRPerkComponent* PerkComponent,
		FName RowName) const;

};
