#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Perk/DRPerkTable.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "DRShopComponent.generated.h"

class APawn;
class UDataTable;
class UDRShopAreaComponent;
class UDRItemDefinition;
class UDRPerkComponent;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 상점 데이터와 거래 범위 참조를 관리하는 컴포넌트를 초기화한다. */
	UDRShopComponent();

	/** 상점에서 사용할 DataTable을 설정하고 Offer 목록을 다시 생성한다. */
	void SetItemTable(UDataTable* NewItemTable);

	/** 상점에서 판매할 퍽 DataTable을 설정한다. */
	void SetPerkTable(UDataTable* NewPerkTable);

	/** 유효한 상점 DataTable이 설정되어 있는지 확인한다. */
	bool HasItemTable() const;

	/** DataTable에서 생성된 구매 및 업그레이드 Offer 목록을 반환한다. */
	const TArray<FDRShopItemOffer>& GetItemOffers() const;

	/** RowName에 해당하는 원본 상점 데이터를 반환한다. */
	bool GetItemRow(FName RowName, FDRShopItemTableRow& OutItemRow) const;

	/** RowName에 해당하는 퍽 정의를 상점의 퍽 테이블에서 반환한다. */
	bool GetPerkRow(FName RowName, FDRPerkTableRow& OutPerkRow) const;

	/** 상점의 퍽 테이블에 등록된 모든 RowName을 반환한다. */
	TArray<FName> GetPerkRowNames() const;

	/** 퍽 정의, 현재 랭크, 가격과 보유 코인을 기준으로 구매 가능 여부를 판단한다. */
	bool CanPurchasePerk(
		FName RowName,
		const UDRPerkComponent* PerkComponent,
		int32 AvailableCoins) const;

	/** 일반 구매 Offer에 포함된 아이템인지 확인한다. */
	bool IsItemAvailable(const UDRItemDefinition* ItemDefinition) const;

	/** 상점 접근 상태와 판매 목록을 기준으로 구매 가능 여부를 확인한다. */
	bool CanPurchase(
		const APawn* Pawn,
		const UDRItemDefinition* ItemDefinition) const;

	/** 플레이어가 현재 상점 범위 안에 있는지 확인한다. */
	bool IsTransactionAllowed(const APawn* Pawn) const;

protected:
	/** 상점 범위 컴포넌트를 찾고 아이템 Offer를 구성한다. */
	virtual void BeginPlay() override;

private:
	/** DataTable 전체를 읽어 런타임 Offer 목록을 재구성한다. */
	void LoadItemOffers();

	/** 일반 상품 또는 단계별 업그레이드 Offer를 목록에 추가한다. */
	void AddItemOffers(FName RowName, const FDRShopItemTableRow& ItemRow);

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRShopItemTableRow"))
	TObjectPtr<UDataTable> ItemTable;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRPerkTableRow"))
	TObjectPtr<UDataTable> PerkTable;

	UPROPERTY(Transient)
	TArray<FDRShopItemOffer> ItemOffers;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopAreaComponent> ShopAreaComponent;
};
