#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRPlayerCameraComponent.generated.h"

class UCameraComponent;
class UCameraShakeBase;
class UDRCharacterMovementComponent;
class USceneComponent;
class USpringArmComponent;

/**
 * 로컬 플레이어 카메라의 추적 보정, 흔들림과 화면 전환을 한 곳에서 관리한다.
 * Scene Component인 CameraBoom / FollowCamera는 소유 Character가 생성하고,
 * 이 컴포넌트는 ConfigureCamera를 통해 필요한 의존성만 주입받는다.
 */
UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRPlayerCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRPlayerCameraComponent();

	void ConfigureCamera(
		USpringArmComponent* InCameraBoom,
		UCameraComponent* InFollowCamera,
		UDRCharacterMovementComponent* InMovementComponent);

	/** 로컬 플레이어 화면에만 카메라 흔들림을 재생한다. */
	UCameraShakeBase* PlayCameraShake(
		TSubclassOf<UCameraShakeBase> ShakeClass,
		float Scale = 1.f) const;

	/** 이 컴포넌트로 재생한 로컬 카메라 흔들림을 중지한다. */
	void StopCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately = false) const;

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCharacterMovementUpdated(
		float DeltaSeconds,
		const FVector& OldLocation,
		const FVector& OldVelocity);

	void UpdateVerticalFollow(bool bAllowInterpolation, float DeltaSeconds);
	void UpdateAutomaticFirstPersonView(float DeltaSeconds);
	void BeginFirstPersonTransition();
	void FinishFirstPersonTransition();
	void UpdateFirstPersonVisualVisibility();
	USceneComponent* FindFirstPersonCameraAnchor() const;
	float GetAvailableThirdPersonCameraDistance() const;
	void ApplyCameraCollisionSettings() const;
	void ApplyTargetLagSettings() const;
	void ApplyCameraBoomLocation() const;
	bool IsLocallyControlledOwner() const;

	/** Spring Arm이 카메라와 지형 사이의 충돌을 검사한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision")
	bool bEnableCameraCollision = true;

	/** 카메라 중심뿐 아니라 근접 클리핑 면까지 지형을 넘지 않도록 검사할 구체 반경. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision", meta = (EditCondition = "bEnableCameraCollision", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float CameraCollisionProbeSize = 24.f;

	/** 카메라 충돌 검사에 사용할 Trace Channel. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision", meta = (EditCondition = "bEnableCameraCollision"))
	TEnumAsByte<ECollisionChannel> CameraCollisionProbeChannel = ECC_Camera;

	/** VoxelWorld가 선택한 카메라 채널을 반드시 Block하도록 보장한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision", meta = (EditCondition = "bEnableCameraCollision"))
	bool bForceVoxelWorldCameraBlocking = true;

	/** 벽 때문에 Spring Arm이 짧아지면 자동으로 1인칭 시점으로 전환한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person")
	bool bEnableAutomaticFirstPerson = true;

	/** BP에 추가한 1인칭 카메라 기준 Scene Component의 이름. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson"))
	FName FirstPersonCameraAnchorName = TEXT("FirstPersonCameraAnchor");

	/** 3인칭 카메라가 확보할 수 있는 거리가 이 값 이하가 되면 1인칭으로 전환한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonEnterDistance = 90.f;

	/** 3인칭 카메라 공간이 이 값 이상 확보되면 3인칭으로 복귀한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonExitDistance = 150.f;

	/** 전환 판단용 Trace 반경. Spring Arm 반경보다 작게 두어 측면 벽의 오판을 줄인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonTransitionProbeSize = 8.f;

	/** 3인칭과 1인칭 위치 사이를 따라가는 보간 응답 속도. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0"))
	float FirstPersonBlendSpeed = 8.f;

	/** 블렌드 비율이 이 값 이상이면 로컬 캐릭터 본체를 숨긴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float FirstPersonHideVisualAlpha = 0.65f;

	/** 블렌드 비율이 이 값 이하가 되면 로컬 캐릭터 본체를 다시 표시한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float FirstPersonShowVisualAlpha = 0.35f;

	/** 캐릭터 이동을 카메라가 약간 늦게 따라가도록 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Target Lag")
	bool bEnableTargetPositionLag = true;

	/** 값이 클수록 카메라가 이동 타겟을 더 빠르게 따라간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Target Lag", meta = (EditCondition = "bEnableTargetPositionLag", ClampMin = "0.0", UIMin = "0.0"))
	float TargetPositionLagSpeed = 10.f;

	/** 이동 타겟과 카메라 피벗 사이에 허용할 최대 거리. 0이면 제한하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Target Lag", meta = (EditCondition = "bEnableTargetPositionLag", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float TargetPositionLagMaxDistance = 100.f;

	/** 시점 회전을 카메라가 약간 늦게 따라가도록 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Target Lag")
	bool bEnableTargetRotationLag = true;

	/** 값이 클수록 카메라가 회전 타겟을 더 빠르게 따라간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Target Lag", meta = (EditCondition = "bEnableTargetRotationLag", ClampMin = "0.0", UIMin = "0.0"))
	float TargetRotationLagSpeed = 12.f;

	/** 프레임 간격이 커져도 랙 보간이 급격히 달라지지 않도록 세부 스텝을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Target Lag", meta = (EditCondition = "bEnableTargetPositionLag || bEnableTargetRotationLag"))
	bool bUseTargetLagSubstepping = true;

	/** 캐릭터의 작은 지형 높이 변화는 카메라에 전달하지 않는 Z축 허용 범위. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float VerticalFollowDeadZone = 25.f;

	/** Z축 허용 범위를 벗어난 뒤 카메라 피벗이 따라가는 속도. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VerticalFollowSpeed = 8.f;

	/** 텔레포트처럼 큰 높이 변화에서는 즉시 카메라 피벗을 재설정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float VerticalFollowSnapDistance = 500.f;

	/** 추적 중 카메라 보정을 멈추는 Z축 오차. 시작 범위보다 작게 유지해야 흔들리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float VerticalFollowReleaseDeadZone = 15.f;

	TWeakObjectPtr<USpringArmComponent> CameraBoom;
	TWeakObjectPtr<UCameraComponent> FollowCamera;
	TWeakObjectPtr<USceneComponent> FirstPersonCameraAnchor;
	TWeakObjectPtr<UDRCharacterMovementComponent> MovementComponent;
	FDelegateHandle MovementUpdatedDelegateHandle;

	FVector CameraBoomBaseRelativeLocation = FVector::ZeroVector;
	float SmoothedCameraPivotZ = 0.f;
	bool bVerticalFollowInitialized = false;
	bool bVerticalFollowActive = false;
	bool bMovementUpdatedSinceLastTick = false;
	bool bFirstPersonTransitionActive = false;
	bool bFirstPersonVisualsHidden = false;
	float FirstPersonBlendAlpha = 0.f;
};
