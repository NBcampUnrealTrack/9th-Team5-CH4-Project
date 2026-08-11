#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRUpgradeComponent.generated.h"

class UDRInventoryComponent;
class UDRItemDefinition;
class UDRShopUIComponent;

USTRUCT()
struct FDRUpgradeOperation
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SourceEntryId;

	UPROPERTY()
	TObjectPtr<UDRItemDefinition> SourceDefinition;

	UPROPERTY()
	TObjectPtr<UDRItemDefinition> TargetDefinition;

	UPROPERTY()
	int32 TargetLevel = 0;
};

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRUpgradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRUpgradeComponent();

	TArray<FDRShopItemOffer> GetNextUpgradeOffers(
		const UDRShopUIComponent* ShopUIComponent,
		const UDRInventoryComponent* Inventory) const;

	bool BuildUpgradeOperation(
		const FDRShopItemTableRow& ItemRow,
		int32 TargetLevel,
		const UDRInventoryComponent* Inventory,
		FDRUpgradeOperation& OutOperation) const;

	bool ApplyUpgrade(
		UDRInventoryComponent* Inventory,
		const FDRUpgradeOperation& Operation) const;

private:
	int32 GetOwnedUpgradeLevel(
		FName RowName,
		const UDRShopUIComponent* ShopUIComponent,
		const UDRInventoryComponent* Inventory) const;

	const FDRShopItemOffer* FindUpgradeOffer(
		FName RowName,
		int32 TargetLevel,
		const UDRShopUIComponent* ShopUIComponent) const;

	bool HasAnyItemInUpgradeChain(
		const FDRShopItemTableRow& ItemRow,
		const UDRInventoryComponent* Inventory) const;

	bool FindUpgradeSourceEntryId(
		const UDRInventoryComponent* Inventory,
		const UDRItemDefinition* SourceDefinition,
		FGuid& OutEntryId) const;
};
