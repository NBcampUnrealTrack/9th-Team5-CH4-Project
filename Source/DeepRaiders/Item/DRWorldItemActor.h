// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRItemInstance.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DRWorldItemActor.generated.h"

class APawn;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EDRWorldItemState : uint8
{
	Dropped,
	Thrown
};

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

	// 투척자만 충돌에서 제외하고 Thrown 상태로 전환한다.
	void MarkAsThrown(APawn* Thrower);

protected:	
	virtual void BeginPlay() override;
	
	// 인벤토리에 넣을 수 있는 액터인가 검증
	virtual bool IsPickupAvailable() const;
	// 획득 후 월드 아이템 제거
	virtual bool FinalizePickup();
	
	void ResetInteractionState();
	void BroadcastMined();
	void BroadcastDropped();
	void ArmGroundHitEvent();
	
	UFUNCTION()
	void OnRep_ItemInstance();

	UFUNCTION()
	void OnRep_WorldItemState();

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

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WorldItemState, Category = "Item")
	EDRWorldItemState WorldItemState = EDRWorldItemState::Dropped;

	UPROPERTY(ReplicatedUsing = OnRep_WorldItemState)
	TObjectPtr<APawn> ThrowingPawn;
	
private:
	// ItemInstance 갱신 시마다 호출
	// MeshData 갱신
	void RefreshItemPresentation();
	void ApplyWorldItemCollision();
	bool bGroundHitEventArmed = false;
	TWeakObjectPtr<APawn> IgnoredThrower;
	
#pragma region Interactable
public:
	bool CanInteract_Implementation(APawn* Interactor) const override;
	bool Interact_Implementation(APawn* Interactor) override;
   
protected:
	uint8 bInteractionInProgress:1 = false;
#pragma endregion
};
