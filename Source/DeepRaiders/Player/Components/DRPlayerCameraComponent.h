#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRPlayerCameraComponent.generated.h"

class UCameraComponent;
class UCameraShakeBase;
class UDRCharacterMovementComponent;
class USceneComponent;
class USpringArmComponent;

UENUM()
enum class EDRCameraPerspectiveState : uint8
{
	ThirdPerson,
	EnteringFirstPerson,
	FirstPerson,
	ExitingFirstPerson
};

UENUM(BlueprintType)
enum class EDRPlayerCameraState : uint8
{
	Default,
	ThrowAim
};

USTRUCT(BlueprintType)
struct FDRPlayerCameraStateSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetArmLength = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (Units = "cm"))
	FVector SocketOffset = FVector(0.f, 90.f, 30.f);
};

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
	void UpdateCameraState(float DeltaSeconds);
	void UpdateAutomaticFirstPersonView(float DeltaSeconds);
	void UpdateCameraSpace(float DeltaSeconds);
	void UpdatePerspectiveState(float DeltaSeconds);
	void BeginFirstPersonTransition(bool bEnteringFirstPerson);
	void FinishFirstPersonTransition();
	void UpdateFirstPersonVisualVisibility();
	USceneComponent* FindFirstPersonCameraAnchor() const;
	FTransform GetDesiredThirdPersonCameraTransform(float ArmLength, const FVector& SocketOffset) const;
	float EvaluateCameraSpace(
		const FVector& TraceStart,
		const FTransform& DesiredCameraTransform,
		FVector* OutCentralCameraLocation = nullptr,
		bool* OutCentralPathBlocked = nullptr) const;
	bool SweepCameraPath(
		const FVector& TraceStart,
		const FVector& TraceEnd,
		float ProbeRadius,
		FVector& OutSafeLocation) const;
	bool IsCameraLocationBlocked(const FVector& CameraLocation) const;
	FVector GetCameraCollisionOrigin() const;
	int32 CountOpenSurroundingCameraPaths() const;
	void ApplyResolvedThirdPersonCamera();
	FTransform GetFirstPersonCameraTransform() const;
	void DrawCameraDebug() const;
	void ApplyCameraCollisionSettings() const;
	void ApplyTargetLagSettings() const;
	void ApplyCameraBoomLocation() const;
	EDRPlayerCameraState ResolveDesiredCameraState() const;
	const FDRPlayerCameraStateSettings& GetCameraStateSettings(EDRPlayerCameraState CameraState) const;
	bool IsLocallyControlledOwner() const;

	/** 투척 조준 중 사용할 3인칭 오버숄더 카메라 설정이다. 자동 1인칭 전환이 활성화되면 1인칭이 우선한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|State")
	FDRPlayerCameraStateSettings ThrowAimCameraSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|State", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraStateBlendSpeed = 5.f;

	/** 카메라 컴포넌트가 지형 충돌을 직접 검사한다. Spring Arm 자체 충돌은 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision")
	bool bEnableCameraCollision = true;

	/** 카메라 중심뿐 아니라 근접 클리핑 면까지 지형을 넘지 않도록 검사할 구체 반경. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision", meta = (EditCondition = "bEnableCameraCollision", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float CameraCollisionProbeSize = 18.f;

	/** 카메라 충돌 검사에 사용할 Trace Channel. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision", meta = (EditCondition = "bEnableCameraCollision"))
	TEnumAsByte<ECollisionChannel> CameraCollisionProbeChannel = ECC_Camera;

	/** VoxelWorld가 선택한 카메라 채널을 반드시 Block하도록 보장한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Collision", meta = (EditCondition = "bEnableCameraCollision"))
	bool bForceVoxelWorldCameraBlocking = true;

	/** 여러 카메라 경로에서 실제 공간 부족이 지속될 때만 1인칭 시점으로 전환한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person")
	bool bEnableAutomaticFirstPerson = true;

	/** BP에 추가한 1인칭 카메라 기준 Scene Component의 이름. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson"))
	FName FirstPersonCameraAnchorName = TEXT("FirstPersonCameraAnchor");

	/** 이 거리 이하의 좁은 공간이 일정 시간 유지될 때만 1인칭으로 전환한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonEnterDistance = 45.f;

	/** 3인칭 카메라 공간이 이 값 이상 확보되면 3인칭으로 복귀한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonExitDistance = 120.f;

	/** 좁은 공간이 이 시간 동안 유지되어야 1인칭 전환을 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FirstPersonEnterHoldTime = 0.10f;

	/** 넓은 공간이 이 시간 동안 유지되어야 3인칭 복귀를 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FirstPersonExitHoldTime = 0.12f;

	/** 좁은 3인칭에서 1인칭으로 이동하는 데 걸리는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0"))
	float FirstPersonEnterBlendDuration = 0.18f;

	/** 1인칭에서 현재의 가까운 3인칭으로 복귀하는 데 걸리는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0"))
	float FirstPersonExitBlendDuration = 0.25f;

	/** 실제 카메라가 이 거리보다 가까워지면 로컬 캐릭터 본체를 숨긴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float CharacterHideDistance = 75.f;

	/** 숨긴 로컬 캐릭터 본체를 다시 표시할 실제 카메라 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float CharacterShowDistance = 110.f;

	/** 기본 3인칭 거리. ConfigureCamera 시 SpringArm의 기존 값으로 초기화한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DefaultThirdPersonArmLength = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float CameraCollisionMargin = 8.f;

	/** 실제 공간 크기를 판정하기 위해 검사할 첫 좌우 각도. 카메라 위치에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg"))
	float CameraAvoidanceYawStepDegrees = 15.f;

	/** 실제 공간 크기를 판정하기 위해 검사할 최대 좌우 각도. 카메라 위치에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg"))
	float CameraAvoidanceMaxYawDegrees = 30.f;

	/** 실제 공간 크기를 판정하기 위해 검사할 위쪽 각도. 카메라 위치에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "deg"))
	float CameraAvoidancePitchDegrees = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraDistanceShrinkFilterSpeed = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraDistanceExpandFilterSpeed = 4.f;

	/** 기존 카메라 위치가 안전할 때 일시적인 복셀 Hit를 무시할 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CameraObstructionConfirmTime = 0.08f;

	/** 충돌로 가까워진 카메라가 다시 멀어지는 속도. 회전 방향에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraCollisionRecoverySpeed = 6.f;

	/** 이 시간 동안 충돌 경로가 연속으로 열려 있어야 카메라 거리를 다시 늘린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Adaptive Third Person", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CameraCollisionRecoveryDelay = 0.20f;

	/** 1인칭 판정을 위해 캐릭터 주변을 검사할 수평 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonSurroundingProbeDistance = 140.f;

	/** 이 거리 이상 확보된 주변 경로를 열린 방향으로 센다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FirstPersonSurroundingOpenDistance = 100.f;

	/** 이 개수 이상의 주변 방향이 열려 있으면 단일 벽으로 판단해 1인칭 진입을 막는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "8"))
	int32 FirstPersonMinimumOpenDirections = 3;

	/** 주변 8방향 검사를 갱신하는 주기. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|First Person", meta = (EditCondition = "bEnableAutomaticFirstPerson", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FirstPersonSurroundingProbeInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera|Debug")
	bool bDrawCameraDebug = false;

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
	FDRPlayerCameraStateSettings DefaultCameraSettings;
	float CurrentCameraStateArmLength = 450.f;
	FVector CurrentCameraStateSocketOffset = FVector::ZeroVector;
	EDRPlayerCameraState TargetCameraState = EDRPlayerCameraState::Default;
	bool bCameraStateInitialized = false;
	float SmoothedCameraPivotZ = 0.f;
	bool bVerticalFollowInitialized = false;
	bool bVerticalFollowActive = false;
	bool bMovementUpdatedSinceLastTick = false;
	EDRCameraPerspectiveState PerspectiveState = EDRCameraPerspectiveState::ThirdPerson;
	bool bFirstPersonVisualsHidden = false;
	bool bCameraDistanceInitialized = false;
	float FirstPersonBlendAlpha = 0.f;
	float FirstPersonTransitionElapsed = 0.f;
	float FirstPersonEnterConditionElapsed = 0.f;
	float FirstPersonExitConditionElapsed = 0.f;
	float CurrentThirdPersonArmLength = 450.f;
	float TargetThirdPersonArmLength = 450.f;
	float CurrentAvailableCameraDistance = 450.f;
	float RawAvailableCameraDistance = 450.f;
	float SmoothedAvailableCameraDistance = 450.f;
	float ResolvedThirdPersonCameraDistance = 450.f;
	FVector ResolvedThirdPersonCameraLocation = FVector::ZeroVector;
	FQuat ResolvedThirdPersonCameraRotation = FQuat::Identity;
	bool bResolvedThirdPersonCameraInitialized = false;
	float CameraObstructionElapsed = 0.f;
	float CameraClearPathElapsed = 0.f;
	int32 OpenSurroundingCameraPathCount = 8;
	float SurroundingCameraProbeElapsed = 0.f;
	bool bSurroundingCameraProbeInitialized = false;
	FVector TransitionStartCameraLocation = FVector::ZeroVector;
	FQuat TransitionStartCameraRotation = FQuat::Identity;
};
