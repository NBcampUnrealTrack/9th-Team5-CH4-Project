#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "DRShopAreaComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRShopAreaPawnSignature,
	APawn*,
	Pawn);

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShopAreaComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UDRShopAreaComponent();

	UPROPERTY(BlueprintAssignable, Category = "Shop|Area")
	FDRShopAreaPawnSignature OnPawnEntered;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Area")
	FDRShopAreaPawnSignature OnPawnExited;

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
