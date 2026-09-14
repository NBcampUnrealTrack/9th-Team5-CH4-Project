#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DeepRaiders/Shop/DRShopSellTypes.h"
#include "DRShopUIComponent.generated.h"

struct FOnAttributeChangeData;

class AActor;
class ADRPlayerController;
class ADRPlayerState;
class UDRInventoryComponent;
class UDRPerkComponent;
class UDRShopComponent;
class UDRShopTransactionComponent;
class UDRUIManagerSubsystem;
class UDRShopWidget;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 로컬 상점 UI 흐름을 관리하는 컴포넌트를 초기화한다. */
	UDRShopUIComponent();

	/** 지정한 상점의 UI를 열거나 닫는다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	void ToggleShopWidget(AActor* ShopActor);

	/** 지정한 상점이 현재 열려 있으면 UI를 닫는다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	void CloseShop(const AActor* ShopActor);

protected:
	/** 로컬 플레이어 컨트롤러와 UI 관리자를 연결한다. */
	virtual void BeginPlay() override;

	/** 상점 UI와 연결된 이벤트 및 입력 상태를 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 상점 UI와 관련 컴포넌트를 연결한다. */
	void ShowShopWidget(AActor* ShopActor);

	/** 상점 UI와 입력 상태를 정리한다. */
	UFUNCTION()
	void HideShopWidget();

	/** 상점 UI 갱신 이벤트를 연결한다. */
	void BindShopEvents();

	/** 상점 UI 갱신 이벤트 연결을 해제한다. */
	void UnbindShopEvents();

	/** 로컬 ASC에 상점 UI 표시 상태를 기록한다. */
	void SetShopOpenTag(bool bIsOpen) const;

	/** UI에서 선택한 Offer를 서버 거래 컴포넌트로 전달한다. */
	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	/** UI에서 선택한 인벤토리 아이템을 서버 판매 요청으로 전달한다. */
	UFUNCTION()
	void HandleSellRequested(EDRShopSellTargetType TargetType, FGuid InstanceId);

	/** 인벤토리가 변경되면 상품 구매 가능 상태를 갱신한다. */
	UFUNCTION()
	void HandleInventoryChanged();

	/** 보유 퍽 목록이 변경되면 구매 가능한 퍽을 다시 표시한다. */
	UFUNCTION()
	void HandlePerksChanged();

	/** 보유 눈이 변경되면 구매 가능 상태를 다시 계산한다. */
	void HandleSnowGaugeChanged(const FOnAttributeChangeData& Data);

	void HandleBlockingStateChanged(FGameplayTag Tag, int32 NewCount);

	/** 현재 플레이어 상태에 맞춰 지정한 Offer UI를 갱신한다. */
	void RefreshOffers(EDRShopOfferType OfferType);

	UFUNCTION()
	void RefreshCharacterUpgrades();

	void RefreshWeaponUpgrades();

	/** 상점 Offer를 UI 표시용 View 데이터로 변환한다. */
	TArray<FDRShopOfferView> MakeOfferViews(
		const TArray<FDRShopItemOffer>& Offers,
		EDRShopOfferType OfferType) const;

	EDRShopOfferSection ResolveOfferSection(
		const FDRShopItemOffer& Offer) const;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopComponent> ShopComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopTransactionComponent> ShopTransactionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRPerkComponent> PerkComponent;

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerState> PlayerState;

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	TWeakObjectPtr<AActor> ActiveShop;

	bool IsMoveInputBlocked = false;
};
