#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "DRLootTypes.h"
#include "DRLootBoxActor.generated.h"

class UDRLootDropComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void HandleBroken(const FDRBreakableDamageContext& DamageContext) override;
	virtual bool ShouldDeferBrokenDestruction() const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material|Parameters")
	FName BaseColorParameterName = TEXT("BaseColor");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material|Parameters")
	FName EmissiveColorParameterName = TEXT("EmissiveColor");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material")
	float EmissiveIntensity = 0.2f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraComponent> IdleAuraVFXComponent;
	
private:
	void HandleLootSpawnSequenceCompleted();
	void RefreshPresentation();
	void RefreshDynamicMaterialColor();
	void RefreshNiagara();
	
	FLinearColor GetRarityColor(EDRLootTier Rarity);
	FLinearColor GetRarityBaseColor(EDRLootTier Rarity);
	FLinearColor GetRarityEmissiveColor(EDRLootTier Rarity);
	
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	FDelegateHandle LootSpawnSequenceCompletedHandle;
	bool bWaitingForLootSpawnSequence = false;
};
