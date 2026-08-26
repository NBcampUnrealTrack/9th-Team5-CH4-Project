#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "DRLootTypes.h"
#include "DRLootBoxActor.generated.h"

class UDRLootDropComponent;
class USceneComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRLootBoxActor : public ADRBreakableActor
{
	GENERATED_BODY()
	
public:
	ADRLootBoxActor();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	bool SetLootTier(EDRLootTier NewLootTier);
	
	UFUNCTION(BlueprintPure, Category = "Loot")
	EDRLootTier GetLootTier() const
	{
		return LootTier;
	}
	
protected:
	virtual void BeginPlay() override;
	
	virtual void HandleBroken(const FDRBreakableDamageContext& DamageContext) override;
	
	UFUNCTION()
	void OnRep_LootTier();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot", meta = (DisplayName = "On Loot Tier Changed"))
	void BP_OnLootTierChanged(EDRLootTier NewLootTier);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDRLootDropComponent> LootDropComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<USceneComponent> LootSpawnPointComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_LootTier, Category = "Loot")
	EDRLootTier LootTier = EDRLootTier::Common;
};
