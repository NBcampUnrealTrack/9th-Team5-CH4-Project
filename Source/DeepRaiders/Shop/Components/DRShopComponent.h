#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopComponent.generated.h"

class APawn;
class UDataTable;
class UDRInventoryComponent;
class UDRShopAreaComponent;
class UDRItemDefinition;
class UDRPerkComponent;
class UDRPerkDefinition;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 상점 데이터와 거래 범위 참조를 관리하는 컴포넌트를 초기화한다. */
	UDRShopComponent();

	/** DataTable에서 생성된 상품 Offer 목록을 반환한다. */
	const TArray<FDRShopItemOffer>& GetItemOffers() const;

	/** RowName에 해당하는 원본 상점 데이터를 반환한다. */
	bool GetItemRow(FName RowName, FDRShopItemTableRow& OutItemRow) const;

	/** RowName에 등록된 아이템이 퍽이면 해당 Definition을 반환한다. */
	bool GetPerkDefinition(
		FName RowName,
		UDRPerkDefinition*& OutPerkDefinition) const;

	/** 퍽 개수 제한, 가격과 보유 눈을 기준으로 구매 가능 여부를 판단한다. */
	bool CanPurchasePerk(
		const UDRPerkDefinition* PerkDefinition,
		const UDRPerkComponent* PerkComponent,
		float AvailableSnowGauge) const;

	/** 가격과 보유 눈을 기준으로 아이템 비용을 지불할 수 있는지 확인한다. */
	bool CanAfford(
		const UDRItemDefinition* ItemDefinition,
		float AvailableSnowGauge) const;

	/** 판매 목록, 가격, 보유 눈과 인벤토리 공간을 기준으로 구매 가능 여부를 판단한다. */
	bool CanPurchaseItem(
		const UDRInventoryComponent* Inventory,
		UDRItemDefinition* ItemDefinition,
		float AvailableSnowGauge) const;

	/** 플레이어가 현재 상점 범위 안에 있는지 확인한다. */
	bool IsTransactionAllowed(const APawn* Pawn) const;

protected:
	/** 상점 범위 컴포넌트를 찾고 아이템 Offer를 구성한다. */
	virtual void BeginPlay() override;

private:
	/** DataTable 전체를 읽어 런타임 Offer 목록을 재구성한다. */
	void LoadItemOffers();

	/** 상품 Offer를 목록에 추가한다. */
	void AddItemOffers(FName RowName, const FDRShopItemTableRow& ItemRow);

	void AddOffer(
		FName RowName,
		EDRShopOfferType OfferType,
		UDRItemDefinition* ItemDefinition);

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRShopItemTableRow"))
	TObjectPtr<UDataTable> ItemTable;

	UPROPERTY(Transient)
	TArray<FDRShopItemOffer> ItemOffers;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopAreaComponent> ShopAreaComponent;
};
