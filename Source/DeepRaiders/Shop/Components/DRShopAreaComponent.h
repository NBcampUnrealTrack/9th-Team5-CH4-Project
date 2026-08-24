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

	/** Pawn이 상점 상호작용 범위에 들어오면 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "Shop|Area")
	FDRShopAreaPawnSignature OnPawnEntered;

	/** Pawn이 상점 상호작용 범위에서 벗어나면 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "Shop|Area")
	FDRShopAreaPawnSignature OnPawnExited;

private:
	TSet<TWeakObjectPtr<APawn>> OverlappingPawns;

	/** 진입한 액터가 Pawn이면 상점 진입 이벤트를 전달한다. */
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool IsFromSweep,
		const FHitResult& SweepResult);

	/** 이탈한 액터가 Pawn이면 상점 이탈 이벤트를 전달한다. */
	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);
};
