#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMovementActionComponent.generated.h"

class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EDRMovementActionType : uint8
{
	None UMETA(DisplayName = "None"),
	Grapple UMETA(DisplayName = "Grapple"),
	Zipline UMETA(DisplayName = "Zipline"),
};

UENUM(BlueprintType)
enum class EDRMovementActionEndReason : uint8
{
	Completed UMETA(DisplayName = "Completed"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	Invalidated UMETA(DisplayName = "Invalidated"),
	OwnerDeath UMETA(DisplayName = "Owner Death"),
};

UENUM(BlueprintType)
enum class EDRZiplineRideMode : uint8
{
	// 탑승 즉시 입력을 무시하고 목표 Endpoint까지 자동 이동한 뒤 자동으로 하차한다.
	AutoTraverse UMETA(DisplayName = "Auto Traverse"),

	// W/S 입력으로 두 Endpoint 사이를 직접 오간다. Endpoint 도달로는 종료되지 않는다.
	ManualTraverse UMETA(DisplayName = "Manual Traverse"),
};

UENUM(BlueprintType)
enum class EDRZiplineManualControlMode : uint8
{
	// W = 월드에서 더 높은 Endpoint, S = 더 낮은 Endpoint.
	// 이동과 별개로 캐릭터 yaw는 카메라를 따라 수직 Rope 주위를 자유롭게 회전한다.
	Vertical UMETA(DisplayName = "Vertical (W Up / S Down)"),

	// W = 현재 카메라가 Rope 축에서 바라보는 쪽, S = 반대쪽.
	ViewRelative UMETA(DisplayName = "View Relative (W Camera Direction)"),
};

/*
 * 이동 액션이 실행되는 동안 유지되는 런타임 상태
 * 실제 설정값은 이후 아이템 또는 스킬 Definition에서 채워 전달한다.
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRMovementActionState
{
	GENERATED_BODY()

public:
	bool IsActive() const
	{
		return bActive;
	}

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	uint8 bActive : 1 = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	EDRMovementActionType ActionType = EDRMovementActionType::None;

	// 이동 액션의 서버 검증, 로컬 예측값을 구분하기 위한 Id
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	int32 SessionId = 0;

	// 그래플링에서는 훅 위치로, Zipline에서는 목표 Endpoint 위치로 사용한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	FVector_NetQuantize ReferenceLocation = FVector::ZeroVector;

	// 액션이 제공하는 방향성 가속도 크기다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ActionAcceleration = 0.f;

	// 액션 중 허용할 최대 속도다. 0 이하이면 제한하지 않는다.
	// Zipline에서는 ReferenceLocation을 향해 이동하는 목표 속도(ManualTraverse에서는 축을 따라 이동하는 최대 속도)로 사용한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float MaxSpeed = 0.f;

	// 기존 입력 가속도를 액션 중 얼마나 반영할지 결정한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ControlScale = 1.f;

	// 비활성 상태로 복제될 때 마지막 종료 이유를 전달한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	EDRMovementActionEndReason LastEndReason = EDRMovementActionEndReason::Invalidated;

	// Zipline 전용: 이번 탑승에서 실제로 Interact한 Endpoint 위치(축의 시작점)다.
	// ReferenceLocation(LinkedEndpoint 위치)과 함께 ManualTraverse의 이동 축을 이룬다. AutoTraverse는 사용하지 않는다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	FVector_NetQuantize ZiplineStartLocation = FVector::ZeroVector;

	// Zipline 진행 방식. AutoTraverse/ManualTraverse를 구분한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	EDRZiplineRideMode ZiplineRideMode = EDRZiplineRideMode::AutoTraverse;

	// ManualTraverse에서 W/S의 양의 진행 방향을 결정한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	EDRZiplineManualControlMode ZiplineManualControlMode = EDRZiplineManualControlMode::Vertical;

	// Zipline 축 방향 속도가 MaxSpeed에 도달할 때까지 사용할 가속도.
	// 0 이하이면 목표 속도를 즉시 적용한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ZiplineAcceleration = 0.f;

	// ManualTraverse에서 입력을 놓았을 때 0까지 감속하는 크기.
	// 0 이하이면 즉시 정지한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ZiplineBrakingDeceleration = 0.f;

	// AutoTraverse 진입 시 서버가 확정한 시작 속도.
	// 탑승 직전 Velocity 중 진행 방향 성분만 제한적으로 계승한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ZiplineInitialSpeed = 0.f;

	// Zipline 탑승 중 캐릭터 몸이 유지할 월드-space 수평 방향.
	// Auto는 진행 방향, Manual은 탑승 시점에 확정한 고정 방향을 사용한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	FVector_NetQuantizeNormal ZiplineFacingDirection = FVector::ForwardVector;

	// Zipline 캐릭터 SkeletalMesh presentation offset.
	// Gameplay Capsule/Rail에는 적용하지 않는다.
	// Character local 기준 X=Forward, Y=Right, Z=Up이다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	FVector_NetQuantize ZiplineStartRideOffset = FVector::ZeroVector;

	// 현재 Rope는 start/target에 같은 presentation offset을 사용한다.
	// 기존 replicated state 호환을 위해 필드는 유지한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	FVector_NetQuantize ZiplineTargetRideOffset = FVector::ZeroVector;

	FVector GetZiplineFacingForward() const
	{
		FVector Forward = ZiplineFacingDirection;
		Forward.Z = 0.f;
		Forward = Forward.GetSafeNormal();

		return Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
	}

	// Gameplay rail은 항상 실제 Cable A-B 선분이다.
	// Facing/Visual 변화가 rail 위치를 바꾸지 않는다.
	FVector GetZiplineRideStartLocation(const FVector& FacingDirection) const
	{
		return FVector(ZiplineStartLocation);
	}

	FVector GetZiplineRideTargetLocation(const FVector& FacingDirection) const
	{
		return FVector(ReferenceLocation);
	}

	FVector GetZiplineRideStartLocation() const
	{
		return FVector(ZiplineStartLocation);
	}

	FVector GetZiplineRideTargetLocation() const
	{
		return FVector(ReferenceLocation);
	}

	void GetZiplineManualTraverseSegment(FVector& OutAxisStart, FVector& OutAxisEnd) const
	{
		OutAxisStart = GetZiplineRideStartLocation();
		OutAxisEnd = GetZiplineRideTargetLocation();

		if (ZiplineManualControlMode != EDRZiplineManualControlMode::Vertical)
		{
			return;
		}

		if (OutAxisStart.Z > OutAxisEnd.Z + KINDA_SMALL_NUMBER)
		{
			Swap(OutAxisStart, OutAxisEnd);
		}
	}

	FVector GetZiplineManualPositiveAxis() const
	{
		FVector AxisStart;
		FVector AxisEnd;

		GetZiplineManualTraverseSegment(AxisStart, AxisEnd);

		return (AxisEnd - AxisStart).GetSafeNormal();
	}
};

/**
 * 이동 컴포넌트가 액션 이동 계산에 제공하는 입력이다.
 * 위치 이동이나 충돌 처리는 포함하지 않는다.
 */
struct FDRMovementActionSimulationInput
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector InputAcceleration = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	float DeltaTime = 0.f;

	// CharacterMovementComponent::Acceleration을 가공하지 않고 그대로 전달한 값이다.
	// Grapple/AutoTraverse는 사용하지 않는다. ManualTraverse가 W/S 방향을 판단하는 데 사용한다.
	FVector RawAcceleration = FVector::ZeroVector;

	// Attach 보정용 world Velocity와 분리된 Zipline 축 방향 gameplay 속도.
	float ZiplineRailSpeed = 0.f;

	// 현재 Character/Capsule이 실제로 바라보는 world-space 방향.
	// RideOffset을 캐릭터 local 기준으로 변환할 때 사용한다.
	FVector ZiplineFacingDirection = FVector::ForwardVector;
};

/**
 * 액션 컴포넌트가 CharacterMovementComponent에 반환하는 이동 기여도다.
 */
struct FDRMovementActionSimulationOutput
{
	FVector AdditionalAcceleration = FVector::ZeroVector;
	float MaxSpeed = 0.f;
	bool bApplyGravity = true;

	// true이면 기존 가속/중력/속도 clamp 계산을 모두 건너뛰고 OverrideVelocity를 그대로 사용한다.
	bool bOverrideVelocity = false;
	FVector OverrideVelocity = FVector::ZeroVector;

	// Zipline의 독립된 rail speed 상태를 CharacterMovementComponent에 되돌린다.
	bool bUpdateZiplineRailSpeed = false;
	float ZiplineRailSpeed = 0.f;
};

/**
 * 이동이 끝난 뒤 GA가 종료 조건을 판단할 수 있도록 전달하는 결과다.
 */
struct FDRMovementActionSimulationResult
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDRMovementActionEnded, EDRMovementActionEndReason);
DECLARE_MULTICAST_DELEGATE_OneParam(FDRMovementActionSimulated, const FDRMovementActionSimulationResult&);

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRMovementActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRMovementActionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 소유 클라이언트가 서버 응답 전에 이동을 예측할 때 사용한다.
	bool StartPredictedMovementAction(const FDRMovementActionState& NewState);

	// 서버가 검증한 이동 액션 상태를 시작한다.
	bool StartAuthoritativeMovementAction(const FDRMovementActionState& NewState);

	// 현재 권한에 맞는 이동 액션 상태를 종료
	void EndMovementAction(EDRMovementActionEndReason EndReason);

	// 현재 상태를 기반으로 이번 프레임의 이동 기여도를 계산한다.
	void EvaluateMovementContribution(const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const;

	// 이동 계산 이후 현재 위치와 속도를 GA에 전달한다.
	void ReportMovementSimulation(const FVector& Location, const FVector& Velocity);

	bool IsMovementActionActive() const;

	const FDRMovementActionState& GetSimulationActionState() const;

	// AnimBP가 별도 애니메이션 asset 없이도 먼저 연동할 수 있는 Zipline presentation 상태다.
	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	bool IsZiplineActive() const;

	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	EDRZiplineRideMode GetZiplineRideMode() const;

	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	EDRZiplineManualControlMode GetZiplineManualControlMode() const;

	// Manual은 +가 positive axis, -가 반대 방향이다. Auto는 0~MaxSpeed 범위다.
	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	float GetZiplineRailSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	float GetZiplineNormalizedSpeed() const;

	// 실제 이동 중인 signed 방향. 정지 중이면 ZeroVector.
	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	FVector GetZiplineTravelDirection() const;

	// 현재 캐릭터가 실제로 바라보는 Zipline presentation 방향.
	// Manual ViewRelative에서는 이동 방향이 반전되면 실제 회전값도 함께 반영된다.
	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	FVector GetZiplineFacingDirection() const;

	// 현재 Capsule 위치에 대응하는 Cable 위의 world-space grip point.
	// AnimBP의 왼손 IK target으로 사용한다.
	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	FVector GetZiplineGripTargetLocation() const;

	// AnimBP에서 root bone에 적용할 Zipline visual offset.
	// Rope의 RideOffset(Character local)을 현재 SkeletalMesh Component Space로 변환해서 반환한다.
	// Gameplay Capsule/CharacterMovement에는 영향을 주지 않는다.
	UFUNCTION(BlueprintPure, Category = "Player|Animation|Zipline")
	FVector GetZiplinePresentationOffsetComponentSpace() const;

	// 현재 위치가 활성화된 Zipline 액션의 목표 Endpoint에 도달했는지 확인한다.
	// Zipline이 아니거나 비활성 상태면 항상 false를 반환한다.
	bool IsZiplineTargetReached(const FVector& CurrentLocation) const;

	// 소유 클라이언트가 활성화된 Zipline에서 이탈을 요청한다. (예: Space 입력)
	void RequestCancelZipline();

	FDRMovementActionEnded OnMovementActionEnded;
	FDRMovementActionSimulated OnMovementActionSimulated;

protected:
	UFUNCTION()
	void OnRep_AuthoritativeActionState();

private:
	bool IsLocallyControlledOwner() const;

	void ClearPredictedActionState();

	void EvaluateGrappleContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const;

	void EvaluateZiplineContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const;

	void EvaluateZiplineAutoTraverseContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const;

	void EvaluateZiplineManualTraverseContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const;

	// 서버에서 요청한 Zipline SessionId를 검증한 뒤 이탈을 처리한다.
	UFUNCTION(Server, Reliable)
	void ServerRequestCancelZipline(int32 SessionId);

	UAbilitySystemComponent* ResolveOwnerAbilitySystemComponent() const;
	void RefreshZiplineGameplayTags();
	void SetZiplineGameplayTagsActive(bool bActive);
	void ApplyZiplineInitialVelocity(const FDRMovementActionState& State) const;
	void ReconcileLocallyControlledMovementMode();

	void RequestReplicationUpdate() const;

	UPROPERTY(ReplicatedUsing = OnRep_AuthoritativeActionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action", meta = (AllowPrivateAccess = "true"))
	FDRMovementActionState AuthoritativeActionState;

	// 클라이언트에서만 사용하는 예측 상태
	FDRMovementActionState PredictedActionState;

	// 이 컴포넌트가 직접 추가한 Zipline 관련 loose tag만 정확히 한 번 제거하기 위한 로컬 상태다.
	bool bZiplineGameplayTagsApplied = false;
	TWeakObjectPtr<UAbilitySystemComponent> ZiplineTaggedAbilitySystem;
};
