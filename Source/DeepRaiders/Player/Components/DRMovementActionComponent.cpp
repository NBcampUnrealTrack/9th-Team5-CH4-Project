
#include "DRMovementActionComponent.h"

#include "DRCharacterMovementComponent.h"
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

bool UDRMovementActionComponent::StartPredictedMovementAction(const FDRMovementActionState& NewState)
{
	if (!IsLocallyControlledOwner()
		|| !NewState.IsActive() 
		|| NewState.SessionId == 0)
	{
		return false;
	}
	
	PredictedActionState = NewState;
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

void UDRMovementActionComponent::EvaluateZiplineAutoTraverseContribution(const FDRMovementActionState& State,
	const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput.bApplyGravity = false;
	OutOutput.bOverrideVelocity = true;

	const FVector StartLocation = State.ZiplineStartLocation;
	const FVector TargetLocation = State.ReferenceLocation;
	const FVector SegmentDelta = TargetLocation - StartLocation;
	const float SegmentLength = SegmentDelta.Size();

	if (SegmentLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Axis = SegmentDelta / SegmentLength;
	const float SafeDeltaTime = FMath::Max(Input.DeltaTime, KINDA_SMALL_NUMBER);

	// 현재 캐릭터 위치를 Zipline segment 위에 투영한다.
	// 따라서 Endpoint 옆에서 상호작용해도 현재 위치에서 Target으로 대각선 이동하지 않고,
	// 먼저/동시에 실제 Zipline 선에 붙으면서 반대 Endpoint 방향으로 진행한다.
	const float DistanceAlongSegment = FMath::Clamp(
		FVector::DotProduct(Input.Location - StartLocation, Axis),
		0.f,
		SegmentLength);

	const float RemainingDistance = SegmentLength - DistanceAlongSegment;
	const float StepDistance = FMath::Min(State.MaxSpeed * SafeDeltaTime, RemainingDistance);
	const float DesiredDistanceAlongSegment = DistanceAlongSegment + StepDistance;

	const FVector DesiredLocation = StartLocation + Axis * DesiredDistanceAlongSegment;

	// 위치 기반으로 이번 substep의 속도를 역산한다.
	// lateral offset 제거와 Endpoint overshoot 방지를 동시에 처리한다.
	OutOutput.OverrideVelocity = (DesiredLocation - Input.Location) / SafeDeltaTime;
}

void UDRMovementActionComponent::EvaluateZiplineManualTraverseContribution(const FDRMovementActionState& State,
	const FDRMovementActionSimulationInput& Input, FDRMovementActionSimulationOutput& OutOutput) const
{
	OutOutput.bApplyGravity = false;
	OutOutput.bOverrideVelocity = true;

	const FVector EndpointA = State.ZiplineStartLocation;
	const FVector EndpointB = State.ReferenceLocation;
	const FVector SegmentDelta = EndpointB - EndpointA;
	const float SegmentLength = SegmentDelta.Size();

	if (SegmentLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// ManualTraverse의 W/S는 어느 Endpoint에서 탔는지가 아니라 월드 높이를 기준으로 한다.
	// W는 높은 Endpoint 방향, S는 낮은 Endpoint 방향이다.
	// 두 Endpoint의 높이가 사실상 같으면 탑승 Endpoint -> LinkedEndpoint 방향을 W로 사용한다.
	FVector LowerEndpoint = EndpointA;
	FVector UpperEndpoint = EndpointB;

	if (EndpointA.Z > EndpointB.Z + KINDA_SMALL_NUMBER)
	{
		LowerEndpoint = EndpointB;
		UpperEndpoint = EndpointA;
	}

	const FVector TraverseAxis = (UpperEndpoint - LowerEndpoint).GetSafeNormal();

	if (TraverseAxis.IsNearlyZero())
	{
		return;
	}

	// PlayerCharacter는 ManualTraverse 중 W/S 입력을 TraverseAxis 방향으로만 AddMovementInput 한다.
	// 기존 CharacterMovement 입력/네트워크 예측 파이프라인을 그대로 사용하고 여기서는 부호만 읽는다.
	const float InputScalar = FVector::DotProduct(Input.RawAcceleration.GetSafeNormal(), TraverseAxis);
	const float SafeDeltaTime = FMath::Max(Input.DeltaTime, KINDA_SMALL_NUMBER);

	// 현재 위치를 실제 Zipline segment 위의 가장 가까운 위치로 투영한다.
	// 입력이 없어도 이 위치로 보정하므로 줄 옆에서 평행하게 이동하지 않고 segment에 붙어 있게 된다.
	const float DistanceAlongSegment = FMath::Clamp(
		FVector::DotProduct(Input.Location - LowerEndpoint, TraverseAxis),
		0.f,
		SegmentLength);

	float DesiredDistanceAlongSegment = DistanceAlongSegment;

	if (!FMath::IsNearlyZero(InputScalar))
	{
		const float SignedStep = FMath::Sign(InputScalar) * State.MaxSpeed * SafeDeltaTime;
		DesiredDistanceAlongSegment = FMath::Clamp(
			DistanceAlongSegment + SignedStep,
			0.f,
			SegmentLength);
	}

	const FVector DesiredLocation = LowerEndpoint + TraverseAxis * DesiredDistanceAlongSegment;

	// 위치 기반으로 이번 substep의 속도를 역산한다.
	// 이동과 동시에 lateral offset을 제거하고 Endpoint overshoot도 막는다.
	OutOutput.OverrideVelocity = (DesiredLocation - Input.Location) / SafeDeltaTime;
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

	return FVector::DistSquared(CurrentLocation, State.ReferenceLocation) <= KINDA_SMALL_NUMBER;
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
