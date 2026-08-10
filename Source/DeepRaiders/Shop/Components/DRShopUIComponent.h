#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRShopUIComponent.generated.h"

class APawn;
class ADRPlayerState;
class UDRInteractionComponent;
class UDRItemDefinition;
class UDRShopWidget;
class UDataTable;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopUIComponent();

	/** Definition이 이 상점의 판매 상품인지 확인한다. */
	bool IsItemAvailable(const UDRItemDefinition* ItemDefinition) const;

	/** 상품 등록 여부와 플레이어의 상점 범위를 함께 확인한다. */
	bool CanPurchase(
		const APawn* Interactor,
		const UDRItemDefinition* ItemDefinition) const;

	/** 플레이어가 이 상점에서 판매할 수 있는 범위인지 확인한다. */
	bool IsSellAllowed(const APawn* Interactor) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** DataTable의 상점 상품 Definition을 캐시한다. */
	void LoadItemDefinitions();

	UFUNCTION()
	void HandleInteractionEntered(APawn* Interactor);

	UFUNCTION()
	void HandleInteractionExited(APawn* Interactor);

	/** 상점 UI를 제거하고 게임 입력으로 복구한다. */
	UFUNCTION()
	void HideShopWidget();

	/** UI 구매 이벤트를 PlayerState 거래 요청으로 전달한다. */
	UFUNCTION()
	void HandlePurchaseRequested(UDRItemDefinition* ItemDefinition);

	/** UI 전체 판매 이벤트를 PlayerState 거래 요청으로 전달한다. */
	UFUNCTION()
	void HandleSellAllOresRequested();

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRShopItemTableRow"))
	TObjectPtr<UDataTable> ItemTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRItemDefinition>> ItemDefinitions;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerState> PlayerState;
};
