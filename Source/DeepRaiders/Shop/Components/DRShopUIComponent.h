#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRShopUIComponent.generated.h"

class APawn;
class UDRInteractionComponent;
class UDRShopWidget;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopUIComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleInteractionEntered(APawn* Interactor);

	UFUNCTION()
	void HandleInteractionExited(APawn* Interactor);

	UFUNCTION()
	void HideShopWidget();

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionComponent> InteractionComponent;
};
