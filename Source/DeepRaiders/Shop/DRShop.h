#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRShop.generated.h"

class APawn;
class UDRShopComponent;
class UDRShopAreaComponent;
class UDRUpgradeComponent;
class USceneComponent;
class USoundBase;

UCLASS()
class DEEPRAIDERS_API ADRShop : public AActor
{
	GENERATED_BODY()

public:
	ADRShop();

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
	void HandlePawnEntered(APawn* Pawn);

	UFUNCTION()
	void HandlePawnExited(APawn* Pawn);

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopAreaComponent> ShopAreaComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopComponent> ShopComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRUpgradeComponent> UpgradeComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|Sound")
	TObjectPtr<USoundBase> PurchaseSound;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Sound",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TransactionSoundVolume = 1.f;
};
