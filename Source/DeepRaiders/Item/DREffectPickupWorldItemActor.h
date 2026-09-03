#pragma once

#include "CoreMinimal.h"
#include "DRHoveringWorldItemActor.h"
#include "DREffectPickupWorldItemActor.generated.h"

class APawn;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADREffectPickupWorldItemActor : public ADRHoveringWorldItemActor
{
	GENERATED_BODY()
	
public:
	ADREffectPickupWorldItemActor();
	
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual bool Interact_Implementation(APawn* Interactor) override;
	
	virtual bool GetInteractionPromptData_Implementation(APawn* Interactor, FDRInteractionPromptData& OutPromptData) const override;
	
protected:
	virtual void BeginPlay() override;
	virtual void RefreshItemPresentation() override;
	virtual void HandleWorldItemStateChanged() override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Pickup", meta = (ClampMin = "1.0", Units = "cm"))
	float PickupRadius = 100.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Pickup", meta = (ClampMin = "0.1", Units = "s"))
	float PostPickupDestroyDelay = 0.25f;

private:
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayPickupPresentation();
	
	int32 ApplyPickupEffects(APawn* TargetPawn) const;
	void RefreshPickupCollision();
	
	bool bPickupConsumed = false;
};
