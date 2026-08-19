#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopUIComponent.generated.h"

class APawn;
class ADRPlayerState;
class UDRInventoryComponent;
class UDRPerkComponent;
class UDRShopAreaComponent;
class UDRShopComponent;
class UDRShopTransactionComponent;
class UDRShopWidget;
class UDRUpgradeComponent;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 로컬 상점 UI 흐름을 관리하는 컴포넌트를 초기화한다. */
	UDRShopUIComponent();

	/** 상점 범위 안에서 UI를 열거나 닫는다. */
	void ToggleShopWidget();

protected:
	/** 상점에 필요한 컴포넌트와 범위 이벤트를 연결한다. */
	virtual void BeginPlay() override;

	/** 상점 UI와 연결된 이벤트 및 입력 상태를 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 로컬 플레이어가 사용할 수 있는 상점으로 등록한다. */
	UFUNCTION()
	void HandlePawnEntered(APawn* Pawn);

	/** 상점 범위를 벗어나면 열려 있는 UI를 닫는다. */
	UFUNCTION()
	void HandlePawnExited(APawn* Pawn);

	/** 상점 UI와 관련 컴포넌트를 연결한다. */
	void ShowShopWidget();

	/** 상점 UI와 입력 상태를 정리한다. */
	UFUNCTION()
	void HideShopWidget();

	/** UI에서 선택한 Offer를 서버 거래 컴포넌트로 전달한다. */
	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	/** UI의 전체 광물 판매 요청을 서버로 전달한다. */
	UFUNCTION()
	void HandleSellAllOresRequested();

	/** 인벤토리가 변경되면 표시할 다음 업그레이드를 다시 계산한다. */
	UFUNCTION()
	void HandleInventoryChanged();

	/** 퍽 등급이 변경되면 다음 구매 가능 등급을 다시 표시한다. */
	UFUNCTION()
	void HandlePerksChanged();

	/** 보유 코인이 변경되면 퍽 구매 가능 상태를 다시 계산한다. */
	UFUNCTION()
	void HandleCoinsChanged(int32 NewCoins);

	/** 현재 보유 단계에 맞는 업그레이드 Offer로 UI를 갱신한다. */
	void RefreshUpgradeOffers();

	/** 현재 퍽 등급에 맞는 다음 등급 Offer로 UI를 갱신한다. */
	void RefreshPerkOffers();

	/** 아이템 Offer를 UI 표시용 View 데이터로 변환한다. */
	TArray<FDRShopOfferView> MakeOfferViews(
		const TArray<FDRShopItemOffer>& Offers,
		EDRShopOfferType OfferType) const;
	/** 퍽 정의와 플레이어 상태를 조합해 퍽 UI View를 구성한다. */
	TArray<FDRShopOfferView> BuildPerkOfferViews() const;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopAreaComponent> ShopAreaComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopComponent> ShopComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopTransactionComponent> ShopTransactionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRUpgradeComponent> UpgradeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRPerkComponent> PerkComponent;

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerState> PlayerState;

	bool IsMoveInputBlocked = false;
};
