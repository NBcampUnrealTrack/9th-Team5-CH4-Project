#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRShopUIComponent.generated.h"

class APawn;
class ADRShopTestPlayerState;
class UDRInteractionComponent;
class UDRItemDefinition;
class UDRShopWidget;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShopUIComponent();
	bool IsItemAvailable(const UDRItemDefinition* ItemDefinition) const;
	bool CanPurchase(
		const APawn* Interactor,
		const UDRItemDefinition* ItemDefinition) const;

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

	UFUNCTION()
	void HandlePurchaseRequested(UDRItemDefinition* ItemDefinition);

	UPROPERTY(EditDefaultsOnly, Category = "Shop|UI")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop|Data")
	TArray<TObjectPtr<UDRItemDefinition>> ItemDefinitions;

	UPROPERTY(Transient)
	TObjectPtr<UDRShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

	UPROPERTY(Transient)
	TObjectPtr<ADRShopTestPlayerState> TestPlayerState;
};
