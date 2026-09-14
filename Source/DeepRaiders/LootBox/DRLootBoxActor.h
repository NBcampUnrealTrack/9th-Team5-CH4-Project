#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/GameStates/DRGameFlowState.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "DRLootTypes.h"
#include "DRLootBoxActor.generated.h"

class ADRMiningGameStateBase;
class UDRLootDropComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UAbilitySystemComponent;
class AVoxelWorld;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRLootBoxActor : public ADRBreakableActor
{
	GENERATED_BODY()
	
public:
	ADRLootBoxActor();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	bool SetLootTier(EDRLootTier NewLootTier);

	/** 복셀 편집 후 호출한다. LootBox 중심이 빈 복셀이 된 경우에만 표시한다. */
	void UpdateVoxelExposure(AVoxelWorld& VoxelWorld);
	
	UFUNCTION(BlueprintPure, Category = "Loot")
	EDRLootTier GetLootTier() const
	{
		return LootTier;
	}
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void HandleBroken(const FDRBreakableDamageContext& DamageContext) override;
	virtual void ApplyBrokenPresentation() override;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Sound",
		meta = (Categories = "GameplayCue.Sound"))
	FGameplayTag IdleLoopSoundCueTag;
	
private:
	void HandleLootSpawnSequenceCompleted();
	void BindGameFlowState();
	void UnbindGameFlowState();
	void ApplyGameFlowAvailability();
	UFUNCTION()
	void HandleGameFlowStateChanged(EDRGameFlowState GameFlowState);
	void RefreshPresentation();
	void RefreshDynamicMaterialColor();
	void RefreshNiagara();
	void RefreshIdleLoopSound();
	void StopIdleLoopSound();
	void BindLocalPlayerVisibilityTags();
	void UnbindLocalPlayerVisibilityTags();
	void RefreshLocalPlayerVisibility();
	void HandleLocalPlayerVisibilityTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void ApplyVisibility();
	UFUNCTION()
	void OnRep_VoxelExposed();
	
	FLinearColor GetRarityColor(EDRLootTier Rarity);
	FLinearColor GetRarityBaseColor(EDRLootTier Rarity);
	FLinearColor GetRarityEmissiveColor(EDRLootTier Rarity);
	
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(Transient)
	TObjectPtr<ADRMiningGameStateBase> MiningGameState;

	FDelegateHandle LootSpawnSequenceCompletedHandle;
	FTimerHandle LocalPlayerVisibilityBindRetryTimer;
	TWeakObjectPtr<UAbilitySystemComponent> LocalPlayerAbilitySystem;
	FDelegateHandle LocalPlayerDeadTagChangedHandle;
	FDelegateHandle LocalPlayerVoxelContainedTagChangedHandle;
	bool bWaitingForLootSpawnSequence = false;
	bool bHideForContainedDeath = false;
	bool bIsGameFlowAvailable = false;

	UPROPERTY(ReplicatedUsing = OnRep_VoxelExposed)
	bool bIsVoxelExposed = false;
};
