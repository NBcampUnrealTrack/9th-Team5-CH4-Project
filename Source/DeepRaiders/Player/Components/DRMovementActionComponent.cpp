#include "DRMovementActionComponent.h"

#include "DRCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

UDRMovementActionComponent::UDRMovementActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UDRMovementActionComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, AuthoritativeActionState);
}

void UDRMovementActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetZiplineGameplayTagsActive(false);
	Super::EndPlay(EndPlayReason);
}

bool UDRMovementActionComponent::StartPredictedMovementAction(const FDRMovementActionState& NewState)
{
	if (!IsLocallyControlledOwner() || !NewState.IsActive() || NewState.SessionId == 0)
	{
		return false;
	}

	PredictedActionState = NewState;
	RefreshZiplineGameplayTags();
	return true;
}

bool UDRMovementActionComponent::StartAuthoritativeMovementAction(const FDRMovementActionState& NewState)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !NewState.IsActive() || NewState.SessionId == 0)
	{
		return false;
	}

	AuthoritativeActionState = NewState;
	RefreshZiplineGameplayTags();
	RequestReplicationUpdate();

	return true;
}

void UDRMovementActionComponent::EndMovementAction(EDRMovementActionEndReason EndReason)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (OwnerActor->HasAuthority())
	{
		if (!AuthoritativeActionState.IsActive())
		{
			return;
		}

		AuthoritativeActionState.bActive = false;
		AuthoritativeActionState.ActionType = EDRMovementActionType::None;
		AuthoritativeActionState.LastEndReason = EndReason;

		RefreshZiplineGameplayTags();
		RequestReplicationUpdate();

		OnMovementActionEnded.Broadcast(EndReason);
		return;
	}

	if (!IsLocallyControlledOwner() || !PredictedActionState.IsActive())
	{
		return;
	}

	ClearPredictedActionState();
	RefreshZiplineGameplayTags();
	OnMovementActionEnded.Broadcast(EndReason);
}

void UDRMovementActionComponent::EvaluateMovementContribution(const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput = FDRMovementActionSimulationOutput();

	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive())
	{
		return;
	}

	switch (State.ActionType)
	{
	case EDRMovementActionType::Grapple:
		EvaluateGrappleContribution(State, Input, OutOutput);
		return;
	case EDRMovementActionType::Zipline:
		EvaluateZiplineContribution(State, Input, OutOutput);
		return;
	default:
		return;
	}
}

void UDRMovementActionComponent::ReportMovementSimulation(const FVector& Location, const FVector& Velocity)
{
	FDRMovementActionSimulationResult Result;
	Result.Location = Location;
	Result.Velocity = Velocity;

	OnMovementActionSimulated.Broadcast(Result);
}

void UDRMovementActionComponent::EvaluateGrappleContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	const FVector ToReference = FVector(State.ReferenceLocation) - Input.Location;

	if (ToReference.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ReferenceDirection = ToReference.GetSafeNormal();
	const FVector ViewDirection = ResolveOwnerViewDirection();
	const float ViewWeight = FMath::Max(State.ViewDirectionWeight, 0.f);	
	
	FVector PullDirection = ReferenceDirection + ViewDirection * ViewWeight;
	
	// 정반대 방향을 바라봐 혼합 결과가 0에 가까우면 훅 방향을 대체값으로 사용.
	if (!PullDirection.Normalize())
	{
		PullDirection = ReferenceDirection;
	}
	
	const FVector ActionAcceleration = PullDirection * State.ActionAcceleration;
		// 상태의 제어 배율에 따라 입력 가속도를 계산
	const FVector ControlAcceleration = Input.InputAcceleration * State.ControlScale;

	OutOutput.AdditionalAcceleration = ActionAcceleration + ControlAcceleration;
	OutOutput.MaxSpeed = State.MaxSpeed;
}

void UDRMovementActionComponent::EvaluateZiplineContribution(
	const FDRMovementActionState& State, 
	const FDRMovementActionSimulationInput& Input, 
	FDRMovementActionSimulationOutput& OutOutput) const
{
	switch (State.ZiplineRideMode)
	{
	case EDRZiplineRideMode::ManualTraverse:
		EvaluateZiplineManualTraverseContribution(State, Input, OutOutput);
		return;
	case EDRZiplineRideMode::AutoTraverse: default:
		EvaluateZiplineAutoTraverseContribution(State, Input, OutOutput);
		return;
	}
}

void UDRMovementActionComponent::EvaluateZiplineAutoTraverseContribution(
	const FDRMovementActionState& State, 
	const FDRMovementActionSimulationInput& Input, 
	FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput.bApplyGravity = false;
	OutOutput.bOverrideVelocity = true;

	/*
	 * Auto의 gameplay rail은 탑승 순간 확정된 Facing 기준으로 고정한다.
	 *
	 * 캐릭터가 Auto 진행 방향으로 회전하는 동안 실제 Actor Forward를
	 * RideOffset basis로 사용하면 rail 자체가 매 프레임 Rope 주위를 회전한다.
	 * 그러면 OverrideVelocity가 그 움직이는 rail을 계속 따라잡으려 하면서
	 * 탑승 직후 lateral correction/jitter가 발생할 수 있다.
	 *
	 * Auto는 진행 방향이 탑승 시 확정되어 있으므로 State Facing을 사용하면
	 * rail은 고정되고 캐릭터의 visual yaw만 별도로 회전한다.
	 */
	const FVector StartLocation = State.GetZiplineRideStartLocation(State.ZiplineFacingDirection);
	const FVector TargetLocation = State.GetZiplineRideTargetLocation(State.ZiplineFacingDirection);
	const FVector SegmentDelta = TargetLocation - StartLocation;
	const float SegmentLength = SegmentDelta.Size();
	if (SegmentLength <= KINDA_SMALL_NUMBER || State.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Axis = SegmentDelta / SegmentLength;
	const float SafeDeltaTime = FMath::Max(Input.DeltaTime, KINDA_SMALL_NUMBER);
	const float DistanceAlongSegment = 
		FMath::Clamp(FVector::DotProduct(Input.Location - StartLocation, Axis), 0.f, SegmentLength);
	const float CurrentRailSpeed = FMath::Clamp(Input.ZiplineRailSpeed, 0.f, State.MaxSpeed);
	const float DesiredRailSpeed = 
		State.ZiplineAcceleration > KINDA_SMALL_NUMBER ? FMath::Min(CurrentRailSpeed + State.ZiplineAcceleration * SafeDeltaTime, State.MaxSpeed) : State.MaxSpeed;

	OutOutput.bUpdateZiplineRailSpeed = true;
	OutOutput.ZiplineRailSpeed = DesiredRailSpeed;

	const float RemainingDistance = SegmentLength - DistanceAlongSegment;

	// 2cm 안쪽은 도착으로 간주해 끝점 근처의 float/network 미세 보정을 제거한다.
	constexpr float EndpointHoldTolerance = 2.f;

	const bool bAtTarget = RemainingDistance <= EndpointHoldTolerance;

	const float StepDistance = bAtTarget ? RemainingDistance : FMath::Min(DesiredRailSpeed * SafeDeltaTime, RemainingDistance);

	const float DesiredDistanceAlongSegment = DistanceAlongSegment + StepDistance;

	const FVector DesiredLocation = StartLocation + Axis * DesiredDistanceAlongSegment;

	if (bAtTarget)
	{
		OutOutput.ZiplineRailSpeed = 0.f;
	}

	/*
	 * 위치 기반으로 속도를 역산하므로
	 * Rope 옆에서 탑승해도 rail에 붙는 보정과
	 * 축 방향 가속을 한 번에 처리한다.
	 */
	OutOutput.OverrideVelocity = (DesiredLocation - Input.Location) / SafeDeltaTime;
}

void UDRMovementActionComponent::EvaluateZiplineManualTraverseContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput.bApplyGravity = false;
	OutOutput.bOverrideVelocity = true;
	OutOutput.bUpdateZiplineRailSpeed = true;

	FVector AxisStart = State.GetZiplineRideStartLocation(Input.ZiplineFacingDirection);
	FVector AxisEnd = State.GetZiplineRideTargetLocation(Input.ZiplineFacingDirection);
	if (State.ZiplineManualControlMode == EDRZiplineManualControlMode::Vertical && AxisStart.Z > AxisEnd.Z + KINDA_SMALL_NUMBER)
	{
		Swap(AxisStart, AxisEnd);
	}

	const FVector SegmentDelta = AxisEnd - AxisStart;
	const float SegmentLength = SegmentDelta.Size();

	if (SegmentLength <= KINDA_SMALL_NUMBER || State.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		OutOutput.ZiplineRailSpeed = 0.f;
		return;
	}

	const FVector TraverseAxis = SegmentDelta / SegmentLength;
	const float SafeDeltaTime = FMath::Max(Input.DeltaTime, KINDA_SMALL_NUMBER);
	const float InputScalar = FVector::DotProduct(Input.RawAcceleration.GetSafeNormal(), TraverseAxis);
	const float CurrentRailSpeed = FMath::Clamp(Input.ZiplineRailSpeed, -State.MaxSpeed, State.MaxSpeed);

	float DesiredRailSpeed = 0.f;

	if (FMath::IsNearlyZero(InputScalar))
	{
		DesiredRailSpeed = State.ZiplineBrakingDeceleration > KINDA_SMALL_NUMBER ? FMath::FInterpConstantTo(CurrentRailSpeed, 0.f, SafeDeltaTime, State.ZiplineBrakingDeceleration) : 0.f;
	}
	else
	{
		const float TargetRailSpeed = FMath::Sign(InputScalar) * State.MaxSpeed;

		DesiredRailSpeed = State.ZiplineAcceleration > KINDA_SMALL_NUMBER ? FMath::FInterpConstantTo(CurrentRailSpeed, TargetRailSpeed, SafeDeltaTime, State.ZiplineAcceleration) : TargetRailSpeed;
	}

	const float DistanceAlongSegment = FMath::Clamp(FVector::DotProduct(Input.Location - AxisStart, TraverseAxis), 0.f, SegmentLength);

	/*
	 * Endpoint 바깥 방향 입력은 실제 이동이 0이므로 rail speed도 0으로 고정한다.
	 * 따라서 끝에 붙은 상태에서 반대 입력을 주면 즉시 안쪽으로 재가속할 수 있다.
	 */
	if ((DistanceAlongSegment <= KINDA_SMALL_NUMBER && DesiredRailSpeed < 0.f) || (DistanceAlongSegment >= SegmentLength - KINDA_SMALL_NUMBER && DesiredRailSpeed > 0.f))
	{
		DesiredRailSpeed = 0.f;
	}

	OutOutput.ZiplineRailSpeed = DesiredRailSpeed;

	const float DesiredDistanceAlongSegment = FMath::Clamp(DistanceAlongSegment + DesiredRailSpeed * SafeDeltaTime, 0.f, SegmentLength);
	const FVector DesiredLocation = AxisStart + TraverseAxis * DesiredDistanceAlongSegment;

	/*
	 * Attach 보정 속도는 OverrideVelocity에만 존재한다.
	 * gameplay rail speed는 OutOutput.ZiplineRailSpeed로 별도 보존하므로
	 * Rope에 붙는 순간의 큰 보정 Velocity가 다음 프레임 이동 속도로 섞이지 않는다.
	 */
	OutOutput.OverrideVelocity = (DesiredLocation - Input.Location) / SafeDeltaTime;
}

bool UDRMovementActionComponent::IsZiplineTargetReached(const FVector& CurrentLocation) const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive() || State.ActionType != EDRMovementActionType::Zipline || State.ZiplineRideMode != EDRZiplineRideMode::AutoTraverse)
	{
		return false;
	}

	constexpr float EndpointHoldTolerance = 2.f;

	return FVector::DistSquared(CurrentLocation, State.GetZiplineRideTargetLocation()) <= FMath::Square(EndpointHoldTolerance);
}

void UDRMovementActionComponent::RequestCancelZipline()
{
	if (!IsLocallyControlledOwner())
	{
		return;
	}

	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive() || State.ActionType != EDRMovementActionType::Zipline)
	{
		return;
	}

	ServerRequestCancelZipline(State.SessionId);
}

void UDRMovementActionComponent::ServerRequestCancelZipline_Implementation(int32 SessionId)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!AuthoritativeActionState.IsActive() || AuthoritativeActionState.ActionType != EDRMovementActionType::Zipline || AuthoritativeActionState.SessionId != SessionId)
	{
		return;
	}

	EndMovementAction(EDRMovementActionEndReason::Cancelled);

	ACharacter* Character = Cast<ACharacter>(OwnerActor);
	UDRCharacterMovementComponent* Movement = IsValid(Character) ? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;

	if (IsValid(Movement))
	{
		Movement->ExitCustomMovementMode();
	}
}

UAbilitySystemComponent* UDRMovementActionComponent::ResolveOwnerAbilitySystemComponent() const
{
	const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(GetOwner());
	return AbilitySystemOwner != nullptr ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
}

void UDRMovementActionComponent::RefreshZiplineGameplayTags()
{
	const FDRMovementActionState& State = GetSimulationActionState();
	const bool bShouldBeActive = State.IsActive() && State.ActionType == EDRMovementActionType::Zipline;

	SetZiplineGameplayTagsActive(bShouldBeActive);
}

void UDRMovementActionComponent::SetZiplineGameplayTagsActive(bool bActive)
{
	if (bActive == bZiplineGameplayTagsApplied)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = bActive ? ResolveOwnerAbilitySystemComponent() : ZiplineTaggedAbilitySystem.Get();

	if (!IsValid(AbilitySystem) && !bActive)
	{
		AbilitySystem = ResolveOwnerAbilitySystemComponent();
	}

	if (!IsValid(AbilitySystem))
	{
		return;
	}

	if (bActive)
	{
		// Active는 기존 QuickSlot 잠금 정책을 그대로 재사용하고,
		// Zipline은 공격/애니메이션 등 Zipline 전용 정책의 식별자로 사용한다.
		AbilitySystem->AddLooseGameplayTag(DRGameplayTags::State_MovementAction_Active);
		AbilitySystem->AddLooseGameplayTag(DRGameplayTags::State_MovementAction_Zipline);

		ZiplineTaggedAbilitySystem = AbilitySystem;
		bZiplineGameplayTagsApplied = true;

		// 탑승 전에 이미 유지 중이던 공격도 즉시 종료한다.
		// 새 공격 시작은 각 Ability의 ActivationBlockedTags가 막는다.
		// WithTags 한 컨테이너에 Ranged/Melee를 함께 넣으면
		// 둘 다 가진 Ability만 매칭될 수 있으므로 각 그룹을 따로 취소한다.
		FGameplayTagContainer RangedAttackTags;
		RangedAttackTags.AddTag(DRGameplayTags::Ability_Attack_Ranged);
		AbilitySystem->CancelAbilities(&RangedAttackTags);

		FGameplayTagContainer MeleeAttackTags;
		MeleeAttackTags.AddTag(DRGameplayTags::Ability_Attack_Melee);
		AbilitySystem->CancelAbilities(&MeleeAttackTags);
		return;
	}

	AbilitySystem->RemoveLooseGameplayTag(DRGameplayTags::State_MovementAction_Zipline);
	AbilitySystem->RemoveLooseGameplayTag(DRGameplayTags::State_MovementAction_Active);

	bZiplineGameplayTagsApplied = false;
	ZiplineTaggedAbilitySystem.Reset();
}

void UDRMovementActionComponent::ApplyZiplineInitialVelocity(const FDRMovementActionState& State) const
{
	if (!State.IsActive() || State.ActionType != EDRMovementActionType::Zipline)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());

	UDRCharacterMovementComponent* Movement = IsValid(Character) ? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;

	if (!IsValid(Movement))
	{
		return;
	}

	if (State.ZiplineRideMode == EDRZiplineRideMode::ManualTraverse)
	{
		// Manual은 Rope를 잡는 순간 현재 운동량과 이전 이동 입력을 모두 버린다.
		Movement->SetZiplineRailSpeed(0.f);
		Movement->ResetManualZiplineInputState();
		Movement->Velocity = FVector::ZeroVector;
		return;
	}

	const FVector TravelAxis = (State.GetZiplineRideTargetLocation() - State.GetZiplineRideStartLocation()).GetSafeNormal();

	if (TravelAxis.IsNearlyZero())
	{
		Movement->Velocity = FVector::ZeroVector;
		return;
	}

	const float InitialSpeed = FMath::Clamp(State.ZiplineInitialSpeed, 0.f, FMath::Max(State.MaxSpeed, 0.f));

	Movement->SetZiplineRailSpeed(InitialSpeed);

	Movement->Velocity = TravelAxis * InitialSpeed;
}

void UDRMovementActionComponent::ReconcileLocallyControlledMovementMode()
{
	APawn* PawnOwner = Cast<APawn>(GetOwner());

	if (!IsValid(PawnOwner) || !PawnOwner->IsLocallyControlled())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(PawnOwner);
	UDRCharacterMovementComponent* Movement = IsValid(Character) ? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;

	if (!IsValid(Movement))
	{
		return;
	}

	const FDRMovementActionState& State = GetSimulationActionState();

	if (State.IsActive())
	{
		if (!Movement->IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
		{
			Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);
		}

		return;
	}

	if (Movement->IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
	{
		Movement->ExitCustomMovementMode();
	}
}

void UDRMovementActionComponent::RequestReplicationUpdate() const
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->GetIsReplicated())
	{
		return;
	}

	OwnerActor->ForceNetUpdate();
}

bool UDRMovementActionComponent::IsMovementActionActive() const
{
	return GetSimulationActionState().IsActive();
}

bool UDRMovementActionComponent::IsZiplineActive() const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	return State.IsActive() && State.ActionType == EDRMovementActionType::Zipline;
}

EDRZiplineRideMode UDRMovementActionComponent::GetZiplineRideMode() const
{
	return GetSimulationActionState().ZiplineRideMode;
}

EDRZiplineManualControlMode UDRMovementActionComponent::GetZiplineManualControlMode() const
{
	return GetSimulationActionState().ZiplineManualControlMode;
}

float UDRMovementActionComponent::GetZiplineRailSpeed() const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive() || State.ActionType != EDRMovementActionType::Zipline)
	{
		return 0.f;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());

	const UDRCharacterMovementComponent* Movement = 
		IsValid(Character) ? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;

	if (!IsValid(Movement))
	{
		return 0.f;
	}

	/*
	 * Authority / AutonomousProxy는 gameplay rail speed를 직접 갖고 있다.
	 * SimulatedProxy는 별도 rail speed를 복제하지 않으므로 replicated Velocity를
	 * rail axis에 투영해서 animation용 속도를 복원한다.
	 */
	if (Character->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return Movement->GetZiplineRailSpeed();
	}

	const FVector RailAxis = State.ZiplineRideMode == EDRZiplineRideMode::ManualTraverse ? State.GetZiplineManualPositiveAxis() : (FVector(State.ReferenceLocation) - FVector(State.ZiplineStartLocation)).GetSafeNormal();

	if (RailAxis.IsNearlyZero())
	{
		return 0.f;
	}

	float RailSpeed = FVector::DotProduct(Movement->Velocity, RailAxis);

	if (State.ZiplineRideMode == EDRZiplineRideMode::AutoTraverse)
	{
		RailSpeed = FMath::Max(0.f, RailSpeed);
	}

	const float SafeMaxSpeed = FMath::Max(State.MaxSpeed, 0.f);

	return State.ZiplineRideMode == EDRZiplineRideMode::ManualTraverse ? FMath::Clamp(RailSpeed, -SafeMaxSpeed, SafeMaxSpeed) : FMath::Clamp(RailSpeed, 0.f, SafeMaxSpeed);
}

float UDRMovementActionComponent::GetZiplineNormalizedSpeed() const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	if (!IsZiplineActive() || State.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Clamp(FMath::Abs(GetZiplineRailSpeed()) / State.MaxSpeed, 0.f, 1.f);
}

FVector UDRMovementActionComponent::GetZiplineTravelDirection() const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	if (!IsZiplineActive())
	{
		return FVector::ZeroVector;
	}

	const float RailSpeed = GetZiplineRailSpeed();

	if (FMath::IsNearlyZero(RailSpeed))
	{
		return FVector::ZeroVector;
	}

	const FVector RailAxis = State.ZiplineRideMode == EDRZiplineRideMode::ManualTraverse ? State.GetZiplineManualPositiveAxis() : (FVector(State.ReferenceLocation) - FVector(State.ZiplineStartLocation)).GetSafeNormal();

	return RailAxis.IsNearlyZero() ? FVector::ZeroVector : RailAxis * FMath::Sign(RailSpeed);
}

FVector UDRMovementActionComponent::GetZiplineFacingDirection() const
{
	if (!IsZiplineActive())
	{
		return FVector::ZeroVector;
	}

	FVector FacingDirection = GetSimulationActionState().ZiplineFacingDirection;

	if (const AActor* OwnerActor = GetOwner())
	{
		FacingDirection = OwnerActor->GetActorForwardVector();
	}

	FacingDirection.Z = 0.f;
	return FacingDirection.GetSafeNormal();
}

FVector UDRMovementActionComponent::GetZiplineGripTargetLocation() const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive() || State.ActionType != EDRMovementActionType::Zipline)
	{
		return FVector::ZeroVector;
	}

	const AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return FVector::ZeroVector;
	}

	/*
	 * Gameplay Capsule은 실제 Cable A-B rail을 따라 이동한다.
	 * RideOffset은 SkeletalMesh presentation에만 적용하므로
	 * GripTarget에는 섞지 않는다.
	 */
	const FVector RideStart = State.GetZiplineRideStartLocation();

	const FVector RideEnd = State.GetZiplineRideTargetLocation();

	const FVector RideDelta = RideEnd - RideStart;

	const float RideLengthSquared = RideDelta.SizeSquared();

	if (RideLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return FVector(State.ZiplineStartLocation);
	}

	const float Alpha = FMath::Clamp(FVector::DotProduct(OwnerActor->GetActorLocation() - RideStart, RideDelta) / RideLengthSquared, 0.f, 1.f);

	return FMath::Lerp(FVector(State.ZiplineStartLocation), FVector(State.ReferenceLocation), Alpha);
}


FVector UDRMovementActionComponent::GetZiplinePresentationOffsetComponentSpace() const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive() || State.ActionType != EDRMovementActionType::Zipline)
	{
		return FVector::ZeroVector;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const USkeletalMeshComponent* Mesh = Character->GetMesh();

	if (!IsValid(Mesh))
	{
		return FVector::ZeroVector;
	}

	/*
	 * RideOffset은 Character local 기준:
	 * X=Forward, Y=Right, Z=Up.
	 *
	 * AnimGraph의 Transform (Modify) Bone은 Component Space vector를 받으므로:
	 * Character Local -> World -> SkeletalMesh Component Space
	 * 로 vector만 변환한다. Translation이므로 위치(origin)는 포함하지 않는다.
	 *
	 * 이 값은 AnimBP pose에만 적용되기 때문에 CharacterMovement network smoothing이
	 * Mesh Component transform을 다시 써도 지워지지 않는다.
	 */
	const FVector CharacterLocalOffset = FVector(State.ZiplineStartRideOffset);

	const FVector WorldOffset = Character->GetActorTransform().TransformVectorNoScale(CharacterLocalOffset);

	return Mesh->GetComponentTransform().InverseTransformVectorNoScale(WorldOffset);
}

const FDRMovementActionState& UDRMovementActionComponent::GetSimulationActionState() const
{
	if (PredictedActionState.IsActive())
	{
		return PredictedActionState;
	}

	return AuthoritativeActionState;
}

void UDRMovementActionComponent::OnRep_AuthoritativeActionState(
	const FDRMovementActionState& PreviousState)
{
	ACharacter* Character =
	Cast<ACharacter>(GetOwner());

	UDRCharacterMovementComponent* Movement =
		IsValid(Character)
			? Cast<UDRCharacterMovementComponent>(
				Character->GetCharacterMovement())
			: nullptr;

	/*
	 * RepNotify 호출 시 AuthoritativeActionState에는 이미 새 값이 들어 있다.
	 *
	 * PreviousState:
	 *   이번 replication 직전 클라이언트가 가지고 있던 authoritative state
	 *
	 * PredictedActionState:
	 *   로컬 prediction이 존재하는 경우 이전 simulation state가 될 수 있다.
	 */
	const bool bPredictedWasActive =
		PredictedActionState.IsActive();

	const bool bWasActive =
		PreviousState.IsActive()
		|| bPredictedWasActive;

	/*
	 * 이미 같은 Session을 로컬 prediction 중이었다면
	 * 서버 승인 후 InitialVelocity를 다시 적용하면 안 된다.
	 */
	const bool bPredictedSessionMatches =
		AuthoritativeActionState.IsActive()
		&& bPredictedWasActive
		&& AuthoritativeActionState.SessionId
			== PredictedActionState.SessionId;

	/*
	 * 이번 replication 이전에도 동일한 authoritative session을
	 * 이미 simulation 중이었는지 확인한다.
	 *
	 * 동일 session의 일반 상태 갱신에서는
	 * InitialVelocity를 다시 적용하지 않는다.
	 */
	const bool bPreviousAuthoritativeSessionMatches =
		PreviousState.IsActive()
		&& AuthoritativeActionState.IsActive()
		&& PreviousState.SessionId
			== AuthoritativeActionState.SessionId;

	const bool bHadSameSessionBeforeReplication =
		bPredictedSessionMatches
		|| bPreviousAuthoritativeSessionMatches;

	/*
	 * 서버 상태가 다른 세션이면
	 * 기존 predicted state를 폐기하고 서버 상태를 우선한다.
	 */
	if (PredictedActionState.IsActive()
		&& !bPredictedSessionMatches)
	{
		ClearPredictedActionState();
	}

	/*
	 * 같은 세션이면 서버가 prediction을 승인한 것.
	 * 이제 authoritative state를 사용한다.
	 */
	if (bPredictedSessionMatches)
	{
		ClearPredictedActionState();
	}

	/*
	 * 서버가 비활성 상태를 보냈다면
	 * 남아 있는 prediction도 제거한다.
	 */
	if (!AuthoritativeActionState.IsActive())
	{
		ClearPredictedActionState();
	}

	RefreshZiplineGameplayTags();

	const bool bActionIsActive =
		IsMovementActionActive();

	/*
	 * 새 authoritative session이 처음 들어온 owning client.
	 *
	 * 현재 Zipline은 entry prediction을 사용하지 않으므로
	 * 정상적인 새 탑승에서는 여기로 들어온다.
	 *
	 * 특히 Auto에서는 서버가 결정한 ZiplineInitialSpeed로
	 * stale ZiplineRailSpeed를 덮어써야 한다.
	 */
	if (bActionIsActive
		&& !bHadSameSessionBeforeReplication
		&& IsLocallyControlledOwner())
	{
		ApplyZiplineInitialVelocity(
			GetSimulationActionState());
	}

	ReconcileLocallyControlledMovementMode();

	/*
	 * replication 전에는 활성 상태였지만
	 * 서버 상태 적용 후 종료된 경우.
	 */
	if (bWasActive
		&& !IsMovementActionActive())
	{
		OnMovementActionEnded.Broadcast(
			AuthoritativeActionState.LastEndReason);
	}
}

bool UDRMovementActionComponent::IsLocallyControlledOwner() const
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());

	return IsValid(PawnOwner) && PawnOwner->IsLocallyControlled();
}

void UDRMovementActionComponent::ClearPredictedActionState()
{
	PredictedActionState = FDRMovementActionState();
}

FVector UDRMovementActionComponent::ResolveOwnerViewDirection() const
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	
	if (IsValid(PawnOwner))
	{
		const FVector ViewDirection = PawnOwner->GetBaseAimRotation().Vector().GetSafeNormal();
		
		if (!ViewDirection.IsNearlyZero())
		{
			return ViewDirection;
		}
		
		return PawnOwner->GetActorForwardVector().GetSafeNormal();
	}
	
	return FVector::ForwardVector;
}
