
#include "DRMovementActionComponent.h"

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
