#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "GameFramework/PlayerState.h"
#include "DRPlayerState.generated.h"

class FLifetimeProperty;
class UDRInventoryComponent;
class UDRItemDefinition;
class UDRShopUIComponent;
class UDRUpgradeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRCoinsChangedSignature,
	int32,
	NewCoins);

UCLASS()
class DEEPRAIDERS_API ADRPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 더 깊은 채굴 위치만 갱신
	bool UpdateDeepestDigLocation(const FVector& Location);

	UFUNCTION(BlueprintPure, Category = "Player|Mining")
	bool HasDeepestDigLocation() const { return bHasDeepestDigLocation; }

	UFUNCTION(BlueprintPure, Category = "Player|Mining")
	FVector GetDeepestDigLocation() const { return DeepestDigLocation; }

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	bool HasJetpack() const
	{
		return bHasJetpack;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	float GetJetpackFuel() const
	{
		return CurrentJetpackFuel;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	float GetMaxJetpackFuel() const
	{
		return MaxJetpackFuel;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	float GetJetpackFuelRatio() const
	{
		if (MaxJetpackFuel <= 0.f)
		{
			return 0.f;
		}

		return FMath::Clamp(
			CurrentJetpackFuel / MaxJetpackFuel,
			0.f,
			1.f);
	}

	/** 서버에서 플레이어에게 제트팩을 지급한다. */
	void GrantJetpack();

	/** 서버에서 연료를 소비한다. */
	bool ConsumeJetpackFuel(float Amount);
	
	/** 서버에서 제트팩 연료를 최대치까지 충전한다. */
	bool RefillJetpackFuel();

	UFUNCTION(BlueprintPure, Category = "Player|Coin")
	int32 GetCoins() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Coin")
	void SetCoins(int32 NewCoins);

	/** 로컬 상품 요청을 서버 거래 처리로 전달한다. */
	void RequestOffer(
		AActor* ShopActor,
		const FDRShopOfferRequest& Request);

	/** 로컬 전체 판매 요청을 서버 거래 처리로 전달한다. */
	void RequestSellAllOres(AActor* ShopActor);

	UPROPERTY(BlueprintAssignable, Category = "Player|Coin")
	FDRCoinsChangedSignature OnCoinsChanged;

protected:
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Mining")
	bool bHasDeepestDigLocation = false;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Mining")
	FVector_NetQuantize DeepestDigLocation = FVector::ZeroVector;

	/** 모든 플레이어가 알아야 하는 제트팩 보유 상태 */
	UPROPERTY(
		ReplicatedUsing = OnRep_HasJetpack,
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Jetpack")
	bool bHasJetpack = false;

	/** 소유 플레이어 UI에서 사용할 현재 연료 */
	UPROPERTY(
		ReplicatedUsing = OnRep_JetpackFuel,
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Jetpack")
	float CurrentJetpackFuel = 0.f;

	/** 프로토타입에서는 모든 인스턴스가 같은 기본값을 사용한다. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Jetpack")
	float MaxJetpackFuel = 100.f;

	UFUNCTION()
	void OnRep_HasJetpack();

	UFUNCTION()
	void OnRep_JetpackFuel();

	UFUNCTION()
	void OnRep_Coins(int32 PreviousCoins);

	/** 서버 데이터로 구매 또는 업그레이드 요청을 재검증한다. */
	UFUNCTION(Server, Reliable)
	void ServerRequestOffer(
		AActor* ShopActor,
		FDRShopOfferRequest Request);

	/** 판매 가능한 광석을 일괄 제거하고 판매 금액을 지급한다. */
	UFUNCTION(Server, Reliable)
	void ServerSellAllOres(AActor* ShopActor);

	UPROPERTY(
		EditDefaultsOnly,
		ReplicatedUsing = OnRep_Coins,
		Category = "Player|Coin",
		meta = (ClampMin = "0"))
	int32 Coins = 1000;

private:
	/** 소유 PlayerController의 인벤토리를 반환한다. */
	UDRInventoryComponent* GetInventoryComponent() const;

	bool TryPurchase(
		const UDRShopUIComponent* ShopUIComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow);

	bool TryUpgrade(
		const UDRUpgradeComponent* UpgradeComponent,
		UDRInventoryComponent* Inventory,
		const FDRShopItemTableRow& ItemRow,
		int32 TargetLevel);

	/** 판매 가능한 광석 엔트리와 총수량을 수집하고 총금액을 반환한다. */
	int64 CollectSellableOreEntries(
		const UDRInventoryComponent* Inventory,
		TArray<FGuid>& OutEntryIds,
		int32& OutTotalQuantity) const;

	/** 연결된 Pawn의 제트팩 외형을 현재 상태에 맞게 갱신한다. */
	void RefreshJetpackVisualOnPawn();
};
