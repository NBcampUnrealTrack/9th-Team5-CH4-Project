#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DRZiplineEndpoint.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * 레벨에 두 개를 짝지어 배치하는 Zipline의 탑승 지점이다.
 * 별도의 Zipline 본체 Actor 없이, Endpoint 자신이 IDRInteractableInterface로
 * 상호작용을 받아 LinkedEndpoint와 연결된 Auto/Manual Traverse 이동을 시작시킨다.
 */
UCLASS()
class DEEPRAIDERS_API ADRZiplineEndpoint : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()

public:
	ADRZiplineEndpoint();

protected:
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual bool Interact_Implementation(APawn* Interactor) override;

private:
	bool CanStartZiplineRide(APawn* Interactor) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionVolume;

	// 레벨에 배치된 상대 Endpoint. A.LinkedEndpoint = B, B.LinkedEndpoint = A 형태로 서로 지정한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADRZiplineEndpoint> LinkedEndpoint;

	// 이 Endpoint에서 탑승했을 때 사용하는 이동 속도(ManualTraverse에서는 최대 속도)다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
	float ZiplineSpeed = 1200.f;

	// 이 Endpoint에서 탑승했을 때의 진행 방식이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	EDRZiplineRideMode RideMode = EDRZiplineRideMode::AutoTraverse;

	// 이 Endpoint의 실제 Cable/기준점에서 캐릭터 Capsule 이동 위치까지의 월드 공간 Offset이다.
	// A/B 각각 값을 가질 수 있으며, 두 Offset을 연결한 선이 실제 캐릭터 이동선이 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true", Units = "cm"))
	FVector RideOffset = FVector(0.0f, 0.0f, -100.0f);

	// 이 거리보다 LinkedEndpoint와 가까우면 사실상 동일한 위치로 보고 탑승을 거부한다.
	static constexpr float MinZiplineDistance = 50.f;
};
