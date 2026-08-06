#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "DRInteractionComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRInteractedSignature,
	APawn*,
	Interactor);

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRInteractionComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UDRInteractionComponent();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInInteractionRange(const APawn* Interactor) const;
	
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Interact(APawn* Interactor);

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FDRInteractedSignature OnInteracted;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FDRInteractedSignature OnInteractionEntered;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FDRInteractedSignature OnInteractionExited;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool IsFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);
};
