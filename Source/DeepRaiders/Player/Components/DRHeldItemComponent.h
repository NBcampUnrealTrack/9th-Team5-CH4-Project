#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemActionTypes.h"
#include "DRHeldItemComponent.generated.h"

class ADRPlayerCharacter;
class UDRItemDefinition;
class USoundBase;
class UDRMiningComponent;
class UDRItemActionPresentationComponent;
class UAnimInstance;

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRHeldItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRHeldItemComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * QuickSlot이 서버에서 결정한
	 * 현재 손 아이템을 적용한다.
	 */
	void SetHeldItemDefinition(UDRItemDefinition* NewItemDefinition);

	UDRItemDefinition* GetHeldItemDefinition() const
	{
		return HeldItemDefinition;
	}

private:
	ADRPlayerCharacter* GetOwnerCharacter() const;

	TWeakObjectPtr<UDRMiningComponent> MiningComponent;
	TWeakObjectPtr<UDRItemActionPresentationComponent> PresentationComponent;

	void RefreshHeldItemState();
	void RefreshVisual();
	void RefreshMiningSettings();

	void PlayEquipSound();

	UFUNCTION()
	void OnRep_HeldItemDefinition();

	void RefreshAnimationLayer();

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LinkedAnimLayerClass;
	
private:
	UPROPERTY(ReplicatedUsing = OnRep_HeldItemDefinition)
	TObjectPtr<UDRItemDefinition> HeldItemDefinition;

	float NextLocalActionTime = 0.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item|Action", meta = (ClampMin = "0.01"))
	float DigActionCooldown = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item|Sound")
	TObjectPtr<USoundBase> EquipSound;
};
