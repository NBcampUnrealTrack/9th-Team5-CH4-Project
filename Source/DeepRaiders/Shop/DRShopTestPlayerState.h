#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DRShopTestPlayerState.generated.h"

class FLifetimeProperty;
class UDRItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRCoinsChangedSignature,
	int32,
	NewCoins);

UCLASS()
class DEEPRAIDERS_API ADRShopTestPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Shop|Test")
	int32 GetCoins() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Shop|Test")
	void SetCoins(int32 NewCoins);

	void RequestPurchase(
		AActor* ShopActor,
		UDRItemDefinition* ItemDefinition);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Test")
	FDRCoinsChangedSignature OnCoinsChanged;

private:
	UFUNCTION()
	void OnRep_Coins(int32 PreviousCoins);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPurchase(
		AActor* ShopActor,
		UDRItemDefinition* ItemDefinition);

	UPROPERTY(
		EditDefaultsOnly,
		ReplicatedUsing = OnRep_Coins,
		Category = "Shop|Test",
		meta = (ClampMin = "0"))
	int32 Coins = 1000;
};
