#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRLastLandingTeleportActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

/** 접촉한 플레이어를 해당 플레이어의 가장 최근 착지 위치로 되돌린다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRLastLandingTeleportActor : public AActor
{
	GENERATED_BODY()

public:
	ADRLastLandingTeleportActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<UBoxComponent> TriggerVolume;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
