

#include "DRLeashComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDRLeashComponent::UDRLeashComponent()
{
	// ==============================
	// Tick을 사용해서 업데이트 한다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// ===============================
	
	SetIsReplicatedByDefault(true);	
}

void UDRLeashComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// 타겟이 없는 경우 Tick 비활성화
	SetComponentTickEnabled(HasLeashAuthority() && IsValid(LeashTarget));
}

void UDRLeashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);
	
	Super::EndPlay(EndPlayReason);
}

void UDRLeashComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, LeashTarget);
	DOREPLIFETIME(ThisClass, bIsFollowing);
}

bool UDRLeashComponent::TrySetLeashTarget(AActor* NewTarget)
{
	if (!HasLeashAuthority()
		|| !IsValidLeashTarget(NewTarget)
		|| LeashTarget == NewTarget)
	{
		return false;
	}
	
	AActor* PreviousTarget = LeashTarget.Get();
	
	LeashTarget = NewTarget;
	CurrentVelocity = FVector::ZeroVector;
	
	// false로 값 초기화, true로 변경은 Update에서 변경됨
	SetFollowingState(false);
	SetComponentTickEnabled(true);
	
	OnLeashTargetChangedDelegate.Broadcast(PreviousTarget, NewTarget);
	
	RequestReplicationUpdate();
	
	return true;
}

bool UDRLeashComponent::TryClaimLeashTarget(AActor* NewTarget)
{
	if (!HasLeashAuthority()
		|| !IsValidLeashTarget(NewTarget))
	{
		return false;
	}
	
	AActor* CurrentTarget = LeashTarget.Get();
	
	if (IsValid(CurrentTarget))
	{
		return CurrentTarget == NewTarget;
	}
	
	return TrySetLeashTarget(NewTarget);
}

bool UDRLeashComponent::TryReleaseLeashTarget()
{
	if (!HasLeashAuthority())
	{
		return false;
	}
	
	if (!IsValid(LeashTarget))
	{
		ClearLeashTarget();
		return false;
	}
	
	AActor* PreviousTarget = LeashTarget.Get();
	
	LeashTarget = nullptr;
	CurrentVelocity = FVector::ZeroVector;
	
	// false로 값 초기화
	SetFollowingState(false);
	SetComponentTickEnabled(false);
	
	OnLeashTargetChangedDelegate.Broadcast(PreviousTarget, nullptr);
	
	RequestReplicationUpdate();
	
	return true;	
}

void UDRLeashComponent::OnRep_LeashTarget(AActor* PreviousTarget)
{
	OnLeashTargetChangedDelegate.Broadcast(PreviousTarget,	LeashTarget.Get());
}

void UDRLeashComponent::OnRep_IsFollowing()
{
	OnLeashFollowingChangedDelegate.Broadcast(bIsFollowing);
}

bool UDRLeashComponent::HasLeashAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

bool UDRLeashComponent::IsValidLeashTarget(const AActor* Target) const
{
	const AActor* OwnerActor = GetOwner();
	
	return IsValid(OwnerActor) && IsValid(Target) && Target != OwnerActor
		&& Target->GetWorld() == OwnerActor->GetWorld();
}

void UDRLeashComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// 서버에서만 Tick
	if (!HasLeashAuthority())
	{
		return;
	}
	
	if (!IsValid(LeashTarget))
	{
		ClearLeashTarget();
		return;
	}
	
	UpdateLeashMovement(DeltaTime);
}

void UDRLeashComponent::UpdateLeashMovement(float DeltaSeconds)
{
	AActor* OwnerActor = GetOwner();
	AActor* TargetActor	= LeashTarget.Get();
	
	if (!IsValid(OwnerActor)
		|| !IsValid(TargetActor))
	{
		return;
	}
	
	const float SafeStartDistance = FMath::Max(FollowStartDistance, 0.0f);
	const float SafeStopDistance = FMath::Clamp(FollowStopDistance, 0.0f, SafeStartDistance);
	const float DistanceRange = FMath::Max(SafeStartDistance - SafeStopDistance, KINDA_SMALL_NUMBER);
	
	const FVector TargetLocation = TargetActor->GetActorTransform().TransformPositionNoScale(FollowOffset);
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	
	const FVector ToTarget = TargetLocation - OwnerLocation;
	const float Distance = ToTarget.Size();
	
	if (!bIsFollowing)
	{
		// 충분히 멀지 않음
		if (Distance <= SafeStartDistance)
		{
			return;
		}
		
		SetFollowingState(true);
	}
	
	const float DistanceGap = FMath::Max(Distance - SafeStopDistance, 0.0f);
	const float SpeedAlpha = FMath::Clamp(DistanceGap / DistanceRange, 0.0f, 1.0f);
	
	const float DesiredSpeed = MaxFollowSpeed * SpeedAlpha;
	const FVector DesiredVelocity = ToTarget.GetSafeNormal() * DesiredSpeed;
	
	CurrentVelocity = FMath::VInterpTo(CurrentVelocity, DesiredVelocity
		, DeltaSeconds, FMath::Max(FollowVelocityResponse, KINDA_SMALL_NUMBER));
	
	CurrentVelocity = CurrentVelocity.GetClampedToMaxSize(FMath::Max(MaxFollowSpeed, 0.0f));
	
	if (Distance <= SafeStopDistance && CurrentVelocity.IsNearlyZero())
	{
		CurrentVelocity = FVector::ZeroVector;
		SetFollowingState(false);
		return;
	}
	
	const FVector MoveDelta = CurrentVelocity * DeltaSeconds;
	
	if (MoveDelta.IsNearlyZero())
	{
		return;
	}
	
	FHitResult Hit;
	
	// bSweep = true 설정, 벽 관통 방지
	OwnerActor->SetActorLocation(OwnerLocation + MoveDelta, true, &Hit, ETeleportType::None);
	
	if (Hit.bBlockingHit)
	{
		// 벽에 끼이지 않도록
		// 똑똑하게 따라오기 위해선 navigation 기반 이동 필요 - 추후 고려
		CurrentVelocity = FVector::VectorPlaneProject(CurrentVelocity, Hit.Normal);
	}
	
}

void UDRLeashComponent::SetFollowingState(bool bNewFollowing)
{
	if (bIsFollowing == bNewFollowing)
	{
		return;
	}
	
	bIsFollowing = bNewFollowing;
	OnLeashFollowingChangedDelegate.Broadcast(bNewFollowing);
	
	RequestReplicationUpdate();

}

void UDRLeashComponent::RequestReplicationUpdate() const
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) ||
		!OwnerActor->GetIsReplicated())
	{
		return;
	}
	
	OwnerActor->FlushNetDormancy();
	OwnerActor->ForceNetUpdate();
}

void UDRLeashComponent::ClearLeashTarget()
{
	LeashTarget = nullptr;
	CurrentVelocity = FVector::ZeroVector;
	SetFollowingState(false);
	SetComponentTickEnabled(false);
		
	RequestReplicationUpdate();
	
	// 두 매개변수 모두 nullptr로 전달
	OnLeashTargetChangedDelegate.Broadcast(nullptr, nullptr);
}
