#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopUIComponent.generated.h"

class APawn;
class UDRInteractionComponent;
class UDRInventoryComponent;
class UDRShopComponent;
class UDRShopTransactionComponent;
class UDRShopWidget;
class UDRUpgradeComponent;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopUIComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 로컬 플레이어가 상점에 진입하면 UI와 관련 컴포넌트를 연결한다. */
	UFUNCTION()
	void HandleInteractionEntered(APawn* Interactor);

	/** 상점 상호작용 범위를 벗어나면 열려 있는 UI를 닫는다. */
	UFUNCTION()
	void HandleInteractionExited(APawn* Interactor);

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

	/** 현재 보유 단계에 맞는 업그레이드 Offer로 UI를 갱신한다. */
	void RefreshUpgradeOffers();

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopComponent> ShopComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopTransactionComponent> ShopTransactionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRUpgradeComponent> UpgradeComponent;
};
