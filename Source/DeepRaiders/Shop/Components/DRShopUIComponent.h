#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopUIComponent.generated.h"

class APawn;
class ADRPlayerState;
class UDataTable;
class UDRInteractionComponent;
class UDRInventoryComponent;
class UDRItemDefinition;
class UDRShopWidget;
class UDRUpgradeComponent;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopUIComponent();

	const TArray<FDRShopItemOffer>& GetItemOffers() const;
	bool GetItemRow(FName RowName, FDRShopItemTableRow& OutItemRow) const;
	bool IsItemAvailable(const UDRItemDefinition* ItemDefinition) const;
	bool IsTransactionAllowed(const APawn* Interactor) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void LoadItemOffers();
	void AddItemOffers(FName RowName, const FDRShopItemTableRow& ItemRow);
	void RefreshUpgradeOffers();

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

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRShopItemTableRow"))
	TObjectPtr<UDataTable> ItemTable;

	UPROPERTY(Transient)
	TArray<FDRShopItemOffer> ItemOffers;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRUpgradeComponent> UpgradeComponent;

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerState> PlayerState;
};
