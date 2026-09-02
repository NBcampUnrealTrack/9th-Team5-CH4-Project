
#include "DRMovementActionComponent.h"

#include "DRCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
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
	if (!IsLocallyControlledOwner()
		|| !NewState.IsActive() 
		|| NewState.SessionId == 0)
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
	
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !NewState.IsActive()
		|| NewState.SessionId == 0)
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
	
	if (!IsLocallyControlledOwner()
		|| !PredictedActionState.IsActive())
	{
		return;
	}
	
	ClearPredictedActionState();
	RefreshZiplineGameplayTags();
	OnMovementActionEnded.Broadcast(EndReason);
}

void UDRMovementActionComponent::EvaluateMovementContribution(const FDRMovementActionSimulationInput& Input,
	FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput = FDRMovementActionSimulationOutput();
	
	const FDRMovementActionState& State = GetSimulationActionState();
	
	if (!State.IsActive())
	{
		return;
	}
	
	switch (State.ActionType)
	{
	case  EDRMovementActionType::Grapple:
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

void UDRMovementActionComponent::EvaluateGrappleContribution(const FDRMovementActionState& State,
                                                             const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	const FVector ToReference = State.ReferenceLocation - Input.Location;
	const float DistanceSquared = ToReference.SizeSquared();
	
	if (DistanceSquared <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	const FVector ReferenceDirection = ToReference.GetSafeNormal();
	const FVector ActionAcceleration = ReferenceDirection * State.ActionAcceleration;
	
	// 상태의 제어 배율에 따라 입력 가속도를 계산
	const FVector ControlAcceleration = Input.InputAcceleration * State.ControlScale;
	
	OutOutput.AdditionalAcceleration = ActionAcceleration + ControlAcceleration;
	OutOutput.MaxSpeed = State.MaxSpeed;
}

void UDRMovementActionComponent::EvaluateZiplineContribution(const FDRMovementActionState& State,
	const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	switch (State.ZiplineRideMode)
	{
	case EDRZiplineRideMode::ManualTraverse:
		EvaluateZiplineManualTraverseContribution(State, Input, OutOutput);
		return;
	case EDRZiplineRideMode::AutoTraverse:
	default:
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

	const FVector StartLocation =
		State.GetZiplineRideStartLocation();

	const FVector TargetLocation =
		State.GetZiplineRideTargetLocation();

	const FVector SegmentDelta =
		TargetLocation - StartLocation;

	const float SegmentLength =
		SegmentDelta.Size();

	if (SegmentLength <= KINDA_SMALL_NUMBER
		|| State.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Axis =
		SegmentDelta / SegmentLength;

	const float SafeDeltaTime =
		FMath::Max(
			Input.DeltaTime,
			KINDA_SMALL_NUMBER);

	const float DistanceAlongSegment =
		FMath::Clamp(
			FVector::DotProduct(
				Input.Location - StartLocation,
				Axis),
			0.f,
			SegmentLength);

	const float CurrentRailSpeed =
		FMath::Clamp(
			Input.ZiplineRailSpeed,
			0.f,
			State.MaxSpeed);

	const float DesiredRailSpeed =
		State.ZiplineAcceleration > KINDA_SMALL_NUMBER
			? FMath::Min(
				CurrentRailSpeed
					+ State.ZiplineAcceleration
					* SafeDeltaTime,
				State.MaxSpeed)
			: State.MaxSpeed;

	OutOutput.bUpdateZiplineRailSpeed = true;
	OutOutput.ZiplineRailSpeed = DesiredRailSpeed;

	const float RemainingDistance =
		SegmentLength - DistanceAlongSegment;

	const float StepDistance =
		FMath::Min(
			DesiredRailSpeed * SafeDeltaTime,
			RemainingDistance);

	const float DesiredDistanceAlongSegment =
		DistanceAlongSegment + StepDistance;

	const FVector DesiredLocation =
		StartLocation
		+ Axis * DesiredDistanceAlongSegment;

	/*
	 * 위치 기반으로 속도를 역산하므로
	 * Rope 옆에서 탑승해도 rail에 붙는 보정과
	 * 축 방향 가속을 한 번에 처리한다.
	 */
	OutOutput.OverrideVelocity =
		(DesiredLocation - Input.Location)
		/ SafeDeltaTime;
}

void UDRMovementActionComponent::EvaluateZiplineManualTraverseContribution(
	const FDRMovementActionState& State,
	const FDRMovementActionSimulationInput& Input,
	FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput.bApplyGravity = false;
	OutOutput.bOverrideVelocity = true;
	OutOutput.bUpdateZiplineRailSpeed = true;

	FVector AxisStart;
	FVector AxisEnd;

	State.GetZiplineManualTraverseSegment(
		AxisStart,
		AxisEnd);

	const FVector SegmentDelta =
		AxisEnd - AxisStart;

	const float SegmentLength =
		SegmentDelta.Size();

	if (SegmentLength <= KINDA_SMALL_NUMBER
		|| State.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		OutOutput.ZiplineRailSpeed = 0.f;
		return;
	}

	const FVector TraverseAxis =
		SegmentDelta / SegmentLength;

	const float SafeDeltaTime =
		FMath::Max(
			Input.DeltaTime,
			KINDA_SMALL_NUMBER);

	const float InputScalar =
		FVector::DotProduct(
			Input.RawAcceleration.GetSafeNormal(),
			TraverseAxis);

	const float CurrentRailSpeed =
		FMath::Clamp(
			Input.ZiplineRailSpeed,
			-State.MaxSpeed,
			State.MaxSpeed);

	float DesiredRailSpeed = 0.f;

	if (FMath::IsNearlyZero(InputScalar))
	{
		DesiredRailSpeed =
			State.ZiplineBrakingDeceleration
				> KINDA_SMALL_NUMBER
				? FMath::FInterpConstantTo(
					CurrentRailSpeed,
					0.f,
					SafeDeltaTime,
					State.ZiplineBrakingDeceleration)
				: 0.f;
	}
	else
	{
		const float TargetRailSpeed =
			FMath::Sign(InputScalar)
			* State.MaxSpeed;

		DesiredRailSpeed =
			State.ZiplineAcceleration
				> KINDA_SMALL_NUMBER
				? FMath::FInterpConstantTo(
					CurrentRailSpeed,
					TargetRailSpeed,
					SafeDeltaTime,
					State.ZiplineAcceleration)
				: TargetRailSpeed;
	}

	const float DistanceAlongSegment =
		FMath::Clamp(
			FVector::DotProduct(
				Input.Location - AxisStart,
				TraverseAxis),
			0.f,
			SegmentLength);

	/*
	 * Endpoint 바깥 방향 입력은 실제 이동이 0이므로 rail speed도 0으로 고정한다.
	 * 따라서 끝에 붙은 상태에서 반대 입력을 주면 즉시 안쪽으로 재가속할 수 있다.
	 */
	if ((DistanceAlongSegment <= KINDA_SMALL_NUMBER
			&& DesiredRailSpeed < 0.f)
		|| (DistanceAlongSegment
				>= SegmentLength - KINDA_SMALL_NUMBER
			&& DesiredRailSpeed > 0.f))
	{
		DesiredRailSpeed = 0.f;
	}

	OutOutput.ZiplineRailSpeed =
		DesiredRailSpeed;

	const float DesiredDistanceAlongSegment =
		FMath::Clamp(
			DistanceAlongSegment
				+ DesiredRailSpeed
				* SafeDeltaTime,
			0.f,
			SegmentLength);

	const FVector DesiredLocation =
		AxisStart
		+ TraverseAxis
		* DesiredDistanceAlongSegment;

	/*
	 * Attach 보정 속도는 OverrideVelocity에만 존재한다.
	 * gameplay rail speed는 OutOutput.ZiplineRailSpeed로 별도 보존하므로
	 * Rope에 붙는 순간의 큰 보정 Velocity가 다음 프레임 이동 속도로 섞이지 않는다.
	 */
	OutOutput.OverrideVelocity =
		(DesiredLocation - Input.Location)
		/ SafeDeltaTime;
}

bool UDRMovementActionComponent::IsZiplineTargetReached(const FVector& CurrentLocation) const
{
	const FDRMovementActionState& State = GetSimulationActionState();

	// ManualTraverse는 Endpoint 도달이 종료 조건이 아니므로 항상 false를 반환한다.
	if (!State.IsActive()
		|| State.ActionType != EDRMovementActionType::Zipline
		|| State.ZiplineRideMode != EDRZiplineRideMode::AutoTraverse)
	{
		return false;
	}

	return FVector::DistSquared(CurrentLocation, State.GetZiplineRideTargetLocation()) <= KINDA_SMALL_NUMBER;
}

void UDRMovementActionComponent::RequestCancelZipline()
{
	if (!IsLocallyControlledOwner())
	{
		return;
	}

	const FDRMovementActionState& State = GetSimulationActionState();

	if (!State.IsActive()
		|| State.ActionType != EDRMovementActionType::Zipline)
	{
		return;
	}

	ServerRequestCancelZipline(State.SessionId);
}

void UDRMovementActionComponent::ServerRequestCancelZipline_Implementation(int32 SessionId)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!AuthoritativeActionState.IsActive()
		|| AuthoritativeActionState.ActionType != EDRMovementActionType::Zipline
		|| AuthoritativeActionState.SessionId != SessionId)
	{
		return;
	}

	EndMovementAction(EDRMovementActionEndReason::Cancelled);

	ACharacter* Character = Cast<ACharacter>(OwnerActor);

	UDRCharacterMovementComponent* Movement = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;

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

	UAbilitySystemComponent* AbilitySystem = bActive
		? ResolveOwnerAbilitySystemComponent()
		: ZiplineTaggedAbilitySystem.Get();

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

void UDRMovementActionComponent::ApplyZiplineInitialVelocity(
	const FDRMovementActionState& State) const
{
	if (!State.IsActive()
		|| State.ActionType != EDRMovementActionType::Zipline)
	{
		return;
	}

	ACharacter* Character =
		Cast<ACharacter>(GetOwner());

	UDRCharacterMovementComponent* Movement =
		IsValid(Character)
			? Cast<UDRCharacterMovementComponent>(
				Character->GetCharacterMovement())
			: nullptr;

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

	const FVector TravelAxis =
		(
			State.GetZiplineRideTargetLocation()
			- State.GetZiplineRideStartLocation()
		)
		.GetSafeNormal();

	if (TravelAxis.IsNearlyZero())
	{
		Movement->Velocity = FVector::ZeroVector;
		return;
	}

	const float InitialSpeed =
		FMath::Clamp(
			State.ZiplineInitialSpeed,
			0.f,
			FMath::Max(
				State.MaxSpeed,
				0.f));

	Movement->SetZiplineRailSpeed(
		InitialSpeed);

	Movement->Velocity =
		TravelAxis * InitialSpeed;
}

void UDRMovementActionComponent::ReconcileLocallyControlledMovementMode()
{
	APawn* PawnOwner = Cast<APawn>(GetOwner());

	if (!IsValid(PawnOwner)
		|| !PawnOwner->IsLocallyControlled())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(PawnOwner);
	UDRCharacterMovementComponent* Movement = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;

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
	
	if (!IsValid(OwnerActor)
		|| !OwnerActor->GetIsReplicated())
	{
		return;
	}
	
	OwnerActor->ForceNetUpdate();
}

bool UDRMovementActionComponent::IsMovementActionActive() const
{
	return GetSimulationActionState().IsActive();
}

const FDRMovementActionState& UDRMovementActionComponent::GetSimulationActionState() const
{
	if (PredictedActionState.IsActive())
	{
		return PredictedActionState;
	}
	
	return AuthoritativeActionState;
}

void UDRMovementActionComponent::OnRep_AuthoritativeActionState()
{
	// 복제 상태 반영 전, 클라이언트 상태 저장
	const bool bWasActive = IsMovementActionActive();
	
	const bool bPredictedSessionMatches = 
		AuthoritativeActionState.IsActive() 
		&& PredictedActionState.IsActive()
		&& AuthoritativeActionState.SessionId == PredictedActionState.SessionId;

	// 서버 상태가 다른 세션이면 서버 상태를 우선한다.
	if (PredictedActionState.IsActive()
		&& !bPredictedSessionMatches)
	{
		ClearPredictedActionState();
	}

	// 같은 세션이면 서버가 예측을 승인한 것으로 간주한다.
	if (bPredictedSessionMatches)
	{
		ClearPredictedActionState();
	}

	// 서버가 비활성 상태를 보냈다면 로컬 예측도 폐기한다.
	if (!AuthoritativeActionState.IsActive())
	{
		ClearPredictedActionState();
	}

	RefreshZiplineGameplayTags();

	if (!bWasActive
		&& IsLocallyControlledOwner())
	{
		ApplyZiplineInitialVelocity(
			GetSimulationActionState());
	}

	ReconcileLocallyControlledMovementMode();

	// 기존에는 활성 상태였지만 서버 상태 반영 후 종료된 경우다.
	if (bWasActive && !IsMovementActionActive())
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
