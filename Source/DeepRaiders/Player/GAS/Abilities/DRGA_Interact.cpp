// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGA_Interact.h"

#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"

UDRGA_Interact::UDRGA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UDRGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle,ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	APawn* Interactor = Cast<APawn>(ActorInfo->AvatarActor.Get());
	AActor* Target = IsValid(Interactor) ? FindBestInteractionTarget(Interactor) : nullptr;
	
	const bool bInteracted = IsValid(Target) && IDRInteractableInterface::Execute_CanInteract(Target, Interactor)
		&& IDRInteractableInterface::Execute_Interact(Target, Interactor);
	
	EndAbility(Handle, ActorInfo,ActivationInfo, true, !bInteracted);
}

AActor* UDRGA_Interact::FindBestInteractionTarget(APawn* Interactor) const
{
	if (!IsValid(Interactor)|| MaxInteractionDistance <= 0.f)
	{
		return nullptr;
	}
	
	UWorld* World = Interactor->GetWorld();
	AController* Controller = Interactor->GetController();
	
	if (!IsValid(World) || !IsValid(Controller))
	{
		return nullptr;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	
	FVector PawnLocation;
	APawn* Pawn = Controller->GetPawn();
	if (!IsValid(Pawn))
	{
		return nullptr;
	}
	PawnLocation = Pawn->GetActorLocation();

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return nullptr;
	}
		
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRInteractOverlap), false, Interactor);
	
	// 상호작용 반경 내의 오브젝트 오버랩 검사
	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByChannel(OverlapResults, PawnLocation, FQuat::Identity, DRCollisionChannels::Interaction,
		FCollisionShape::MakeSphere(MaxInteractionDistance), QueryParams);
	
	DrawDebugSphere(GetWorld(), PawnLocation, 300.0f, 16, FColor::Red, false, 3.0f);
	
	const float MinimumAimDot = FMath::Cos(FMath::DegreesToRadians(MaxInteractionAngleDegrees));
	const float MaximumDistanceSquared = FMath::Square(MaxInteractionDistance);
	
	AActor* BestTarget = nullptr;
	float BestAimDot = -1.f;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	
	TSet<AActor*> EvaluatedActors;
	
	// 반경 내의 오브젝트 중 Aim과 가장 근접한 상호작용 액터 탐색
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* Candidate = OverlapResult.GetActor();
		
		if (!IsValid(Candidate)
			|| Candidate == Interactor
			|| EvaluatedActors.Contains(Candidate)
			|| !Candidate->Implements<UDRInteractableInterface>())
		{
			continue;
		}
		
		EvaluatedActors.Add(Candidate);
		
		if (!IDRInteractableInterface::Execute_CanInteract(Candidate, Interactor))
		{
			continue;
		}
		
		FVector BoundsOrigin;
		FVector BoundsExetent;
		Candidate->GetActorBounds(false, BoundsOrigin, BoundsExetent);
		
		const FVector PawnToTarget = BoundsOrigin - PawnLocation;
		const FVector ViewToTarget = BoundsOrigin - ViewLocation;
		const float DistanceSquared = PawnToTarget.SizeSquared();
		
		if (DistanceSquared <= KINDA_SMALL_NUMBER || DistanceSquared > MaximumDistanceSquared)
		{
			continue;
		}
		
		const FVector ViewToTargetDirection = ViewToTarget.GetSafeNormal();
		const float AimDot = FVector::DotProduct(ViewDirection, ViewToTargetDirection);
		
		if (AimDot < MinimumAimDot)
		{
			continue;
		}
		
		if (!HasClearLineOfSight(World, Interactor, Candidate, ViewLocation, BoundsOrigin))
		{
			continue;
		}
		
		/*
		 * 마우스 포인터에 가장 가까운 액터를 선정
		 */
		const bool bCloserToPointer = AimDot > BestAimDot + KINDA_SMALL_NUMBER;
		const bool bSameAngleButCloser = FMath::IsNearlyEqual(AimDot, BestAimDot, KINDA_SMALL_NUMBER)
			&& DistanceSquared < BestDistanceSquared;
		
		if (bCloserToPointer || bSameAngleButCloser)
		{
			BestTarget = Candidate;
			BestAimDot = AimDot;
			BestDistanceSquared = DistanceSquared;
		}
	}
	
	return BestTarget;
}

bool UDRGA_Interact::HasClearLineOfSight(UWorld* World, APawn* Interactor, AActor* Target, const FVector& ViewLocation,
	const FVector& TargetLocation) const
{
	if (!IsValid(World) || !IsValid(Interactor) || !IsValid(Target))
	{
		return false;
	}
	
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRInteractLineOfSight), true, Interactor);
	
	FHitResult HitResult;
	const bool bBlockingHit = World->LineTraceSingleByChannel(HitResult, ViewLocation, TargetLocation,
		ECC_Visibility, QueryParams);
	
	return !bBlockingHit || HitResult.GetActor() == Target;
}
