#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRShop.generated.h"

class APawn;
class UDRShopComponent;
class UDRShopAreaComponent;
class USceneComponent;
class USoundBase;

UCLASS()
class DEEPRAIDERS_API ADRShop : public AActor
{
	GENERATED_BODY()

public:
	ADRShop();

	/** Pawn이 상점 영역 안에 있는지 확인한다. */
	bool IsPawnInShopArea(const APawn* Pawn) const;

	USoundBase* GetPurchaseSound() const
	{
		return PurchaseSound;
	}

	float GetTransactionSoundVolume() const
	{
		return TransactionSoundVolume;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleShopAreaExited(APawn* Pawn);

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopAreaComponent> ShopAreaComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopComponent> ShopComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|Sound")
	TObjectPtr<USoundBase> PurchaseSound;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Sound",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TransactionSoundVolume = 1.f;
};
