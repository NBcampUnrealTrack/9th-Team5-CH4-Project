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
	UFUNCTION()
	void HandleInteractionEntered(APawn* Interactor);

	UFUNCTION()
	void HandleInteractionExited(APawn* Interactor);

	UFUNCTION()
	void HideShopWidget();

	UFUNCTION()
	void HandleOfferRequested(FDRShopOfferRequest Request);

	UFUNCTION()
	void HandleSellAllOresRequested();

	UFUNCTION()
	void HandleInventoryChanged();

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
