// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRItemInstance.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DRWorldItemActor.generated.h"

class APawn;
class UPrimitiveComponent;

UCLASS()
class DEEPRAIDERS_API ADRWorldItemActor : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()
	
public:	
	ADRWorldItemActor();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	const FDRItemInstance& GetItemInstance() const
	{
		return ItemInstance;
	}
	
	bool SetInitialItemInstance(FDRItemInstance InItemInstance);
	
	// 버려지는 순간 적용될 Impulse
	void ApplyDropImpulse(const FVector& Impulse);

protected:	
	virtual void BeginPlay() override;
	
	// 인벤토리에 넣을 수 있는 액터인가 검증
	virtual bool IsPickupAvailable() const;
	// 획득 후 월드 아이템 제거
	virtual bool FinalizePickup();
	
	void ResetInteractionState();
	void BroadcastMined();
	void ArmGroundHitEvent();
	
	UFUNCTION()
	void OnRep_ItemInstance();

	UFUNCTION()
	void HandleStaticMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayActiveSound();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayPickupSound();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDroppedSound();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ItemInstance)
	FDRItemInstance ItemInstance;
	
private:
	// ItemInstance 갱신 시마다 호출
	// MeshData 갱신
	void RefreshItemPresentation();
	bool bGroundHitEventArmed = false;
	
#pragma region Interactable
public:
	bool CanInteract_Implementation(APawn* Interactor) const override;
	bool Interact_Implementation(APawn* Interactor) override;
   
protected:
	uint8 bInteractionInProgress:1 = false;
#pragma endregion
};
