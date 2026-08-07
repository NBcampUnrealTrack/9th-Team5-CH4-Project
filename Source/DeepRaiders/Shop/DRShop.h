#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRShop.generated.h"

class UDRInteractionComponent;
class UDRShopUIComponent;
class USceneComponent;

UCLASS()
class DEEPRAIDERS_API ADRShop : public AActor
{
	GENERATED_BODY()

public:
	ADRShop();

private:
	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UDRShopUIComponent> ShopUIComponent;
};
