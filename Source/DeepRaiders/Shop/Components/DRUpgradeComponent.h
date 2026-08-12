#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRUpgradeComponent.generated.h"

class UDRInventoryComponent;
class UDRItemDefinition;
class UDRShopComponent;

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

	/** 현재 인벤토리를 기준으로 각 업그레이드 체인의 다음 단계만 반환한다. */
	TArray<FDRShopItemOffer> GetNextUpgradeOffers(
		const UDRShopComponent* ShopComponent,
		const UDRInventoryComponent* Inventory) const;

	/** 요청 단계와 보유 아이템을 검증해 서버에서 실행할 작업을 생성한다. */
	bool BuildUpgradeOperation(
		const FDRShopItemTableRow& ItemRow,
		int32 TargetLevel,
		const UDRInventoryComponent* Inventory,
		FDRUpgradeOperation& OutOperation) const;

	/** 검증된 작업에 따라 신규 아이템 추가 또는 기존 Definition 교체를 수행한다. */
	bool ApplyUpgrade(
		UDRInventoryComponent* Inventory,
		const FDRUpgradeOperation& Operation) const;

private:
	/** 업그레이드 체인에서 현재 보유 중인 가장 높은 단계를 반환한다. */
	int32 GetOwnedUpgradeLevel(
		FName RowName,
		const UDRShopComponent* ShopComponent,
		const UDRInventoryComponent* Inventory) const;

	/** RowName과 목표 단계가 일치하는 Offer를 찾는다. */
	const FDRShopItemOffer* FindUpgradeOffer(
		FName RowName,
		int32 TargetLevel,
		const UDRShopComponent* ShopComponent) const;

	/** 동일 업그레이드 체인의 아이템을 하나라도 보유하고 있는지 확인한다. */
	bool HasAnyItemInUpgradeChain(
		const FDRShopItemTableRow& ItemRow,
		const UDRInventoryComponent* Inventory) const;

	/** 교체할 장비 Entry를 찾아 고유 ID를 반환한다. */
	bool FindUpgradeSourceEntryId(
		const UDRInventoryComponent* Inventory,
		const UDRItemDefinition* SourceDefinition,
		FGuid& OutEntryId) const;
};
