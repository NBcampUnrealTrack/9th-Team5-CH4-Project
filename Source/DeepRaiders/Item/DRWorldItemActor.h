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
	Dropped  UMETA(DisplayName = "Dropped"),
	Thrown  UMETA(DisplayName = "Thrown"),
	Emerging UMETA(DisplayName = "Emerging"),
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
	void MulticastPlayPickupSound(APawn* Interactor);
	
	virtual void MulticastPlayPickupSound_Implementation(APawn* Interactor);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDroppedSound();
	
	void SetWorldItemState(EDRWorldItemState NewState);
	
	virtual void HandleWorldItemStateChanged();
	virtual void RefreshItemPresentation();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ItemInstance)
	FDRItemInstance ItemInstance;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WorldItemState, Category = "Item")
	EDRWorldItemState WorldItemState = EDRWorldItemState::Dropped;

	UPROPERTY(ReplicatedUsing = OnRep_WorldItemState)
	TObjectPtr<APawn> ThrowingPawn;

	// 아이템 착지음을 재생할 최소 낙하 높이(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Sound", meta = (ClampMin = "0.0"))
	float MinimumDropSoundHeight = 25.f;
	
private:
	// ItemInstance 갱신 시마다 호출
	void ApplyWorldItemCollision();
	
	bool bGroundHitEventArmed = false;
	float GroundHitArmHeight = 0.f;
	TWeakObjectPtr<APawn> IgnoredThrower;
	
#pragma region Interactable
public:
	bool CanInteract_Implementation(APawn* Interactor) const override;
	bool Interact_Implementation(APawn* Interactor) override;
	bool GetInteractionPromptData_Implementation(APawn* Interactor, FDRInteractionPromptData& OutPromptData) const override;
	
protected:
	uint8 bInteractionInProgress:1 = false;
#pragma endregion
};
