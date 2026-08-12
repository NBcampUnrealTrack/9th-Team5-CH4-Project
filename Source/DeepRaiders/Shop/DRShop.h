#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRShop.generated.h"

class UDRInteractionComponent;
class UDRShopComponent;
class UDRShopUIComponent;
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

	USoundBase* GetSellSound() const
	{
		return SellSound;
	}

	float GetTransactionSoundVolume() const
	{
		return TransactionSoundVolume;
	}

private:
	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopComponent> ShopComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRUpgradeComponent> UpgradeComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopUIComponent> ShopUIComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|Sound")
	TObjectPtr<USoundBase> PurchaseSound;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|Sound")
	TObjectPtr<USoundBase> SellSound;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shop|Sound",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TransactionSoundVolume = 1.f;
};
