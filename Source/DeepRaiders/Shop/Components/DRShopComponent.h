#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopComponent.generated.h"

class APawn;
class UDataTable;
class UDRInteractionComponent;
class UDRItemDefinition;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopComponent();

	void SetItemTable(UDataTable* NewItemTable);
	bool HasItemTable() const;
	const TArray<FDRShopItemOffer>& GetItemOffers() const;
	bool GetItemRow(FName RowName, FDRShopItemTableRow& OutItemRow) const;
	bool IsItemAvailable(const UDRItemDefinition* ItemDefinition) const;
	bool CanPurchase(
		const APawn* Interactor,
		const UDRItemDefinition* ItemDefinition) const;
	bool IsTransactionAllowed(const APawn* Interactor) const;

protected:
	virtual void BeginPlay() override;

private:
	void LoadItemOffers();
	void AddItemOffers(FName RowName, const FDRShopItemTableRow& ItemRow);

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRShopItemTableRow"))
	TObjectPtr<UDataTable> ItemTable;

	UPROPERTY(Transient)
	TArray<FDRShopItemOffer> ItemOffers;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionComponent> InteractionComponent;
};
