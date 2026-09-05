#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DRZiplineRope.generated.h"

class ADRPlayerCharacter;
class ADRZiplineEndpoint;
class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class USplineMeshComponent;
class UStaticMesh;

/**
 * 하나의 완성된 Zipline을 소유하는 Actor.
 *
 * - EndpointA / EndpointB: Cable의 고정 Anchor
 * - CableVisual: 순수 시각 표현 (Collision / Physics 없음)
 * - InteractionVolume: Rope 전체를 E 상호작용 대상으로 만든 Query Volume
 * - Gameplay Rail: Endpoint 사이의 결정론적 직선 segment
 *
 * Rope 자체 물리 시뮬레이션은 사용하지 않는다.
 */
UCLASS()
class DEEPRAIDERS_API ADRZiplineRope : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()

public:
	ADRZiplineRope();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override
	{
		return true;
	}
#endif

protected:
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual bool Interact_Implementation(APawn* Interactor) override;
	virtual bool GetInteractionPromptData_Implementation(APawn* Interactor, FDRInteractionPromptData& OutPromptData) const override;
	virtual bool GetInteractionLocation_Implementation(APawn* Interactor, FVector& OutInteractionLocation) const override;

private:
	bool CanStartZiplineRide(APawn* Interactor) const;
	bool ResolveEndpointLocations(FVector& OutEndpointA, FVector& OutEndpointB) const;
	bool ShouldAutoTargetEndpointB(const ADRPlayerCharacter* Character, const FVector& EndpointA, const FVector& EndpointB) const;
	void OrientEndpointsTowardEachOther();
	void RefreshRopeGeometry();

	static FVector ClosestPointOnSegmentToViewRay(const FVector& SegmentStart, const FVector& SegmentEnd, const FVector& ViewLocation, const FVector& ViewDirection);

	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineMeshComponent> CableVisual;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zipline|Endpoints", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADRZiplineEndpoint> EndpointA;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zipline|Endpoints", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADRZiplineEndpoint> EndpointB;

	// Rope 전체가 공유하는 캐릭터 SkeletalMesh presentation offset.
	// Gameplay Capsule/Cable rail은 바꾸지 않는다.
	// Character local 기준 X = 앞/뒤, Y = 오른쪽/왼쪽, Z = 위/아래.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride", meta = ( AllowPrivateAccess = "true", Units = "cm", ToolTip = "Character-local visual mesh offset. X=Forward, Y=Right, Z=Up."))
	FVector RideOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride", meta = (AllowPrivateAccess = "true"))
	EDRZiplineRideMode RideMode = EDRZiplineRideMode::AutoTraverse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride", meta = ( AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float MaxSpeed = 1200.f;

	// Auto: 현재 진행 속도 -> MaxSpeed.
	// 0이면 기존 방식처럼 MaxSpeed를 즉시 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Auto", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s^2"))
	float AutoAcceleration = 1000.f;

	// Auto 탑승 직전 Velocity에서 진행방향 성분을 얼마까지 계승할지 제한한다.
	// 0이면 항상 정지 상태에서 가속을 시작한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Auto", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
	float AutoMaxEntrySpeed = 800.f;

	// 상승 Auto Zipline은 연속 재탑승으로 고도를 빠르게 확보하지 못하도록 가속도를 낮춘다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Auto|Upward",
		meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardAutoAccelerationMultiplier = 0.3f;

	// 상승 Auto 탑승 시 기존 Velocity에서 계승할 수 있는 진행방향 속도의 상한.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Auto|Upward",
		meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
	float UpwardAutoMaxEntrySpeed = 200.f;

	// TravelAxis.Z가 이 값보다 클 때 상승 Zipline으로 판정한다. 0.2 ~= 약 11.5도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Auto|Upward",
		meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardDirectionThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Manual", meta = (AllowPrivateAccess = "true"))
	EDRZiplineManualControlMode ManualControlMode = EDRZiplineManualControlMode::Vertical;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Manual", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s^2"))
	float ManualAcceleration = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Manual", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s^2"))
	float ManualBrakingDeceleration = 2000.f;

	/**
	 * Rope Mesh는 local X 축 방향으로 길게 제작된 Mesh를 권장한다.
	 * 한 개의 SplineMesh를 Endpoint 사이에 stretch한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> CableMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> CableMaterialOverride;

	// SplineMesh의 Y/Z 단면 Scale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Visual", meta = ( AllowPrivateAccess = "true", ClampMin = "0.01"))
	FVector2D CableScale = FVector2D(1.f, 1.f);

	// Interaction query box의 Rope 축 수직 방향 반폭.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Interaction", meta = ( AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	float InteractionHalfWidth = 75.f;

	// Rope 양 끝에서 Interaction Volume을 조금 더 연장한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Interaction", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float InteractionEndPadding = 30.f;

	// Editor에서 Endpoint 이동 시 불필요한 매 프레임 Component 갱신을 피하기 위한 캐시.
	FVector CachedEndpointA = FVector(TNumericLimits<float>::Max());

	FVector CachedEndpointB = FVector(TNumericLimits<float>::Max());

	static constexpr float MinRopeLength = 50.f;
	static constexpr float AutoEndpointSnapDistance = 30.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Endpoints", meta = (AllowPrivateAccess = "true"))
	bool bAutoOrientEndpoints = true;
	
	// Auto Traverse가 목표 Endpoint 도착 시 자동으로 하차할지 여부.
	// false이면 끝점에서 정지한 채 Zipline 상태를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zipline|Ride|Auto", meta = (AllowPrivateAccess = "true"))
	bool bAutoDismountAtTarget = false;
};
