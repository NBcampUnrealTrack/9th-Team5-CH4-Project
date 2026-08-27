#include "DRSnowRemoveComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "VoxelWorld.h"

UDRSnowRemoveComponent::UDRSnowRemoveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UDRSnowRemoveComponent::TryRemoveSnowFromHit(const FHitResult& HitResult, const FDRSnowRemovalSpec& RemovalSpec)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowFromHit(HitResult, RemovalSpec);
		return 0.f;
	}

	if (!HitResult.bBlockingHit)
	{
		return 0.f;
	}

	if (!CanRemoveNow(RemovalSpec))
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	FDRSnowSurfaceRemoveRequest Request =
		MakeRemoveRequest(HitResult.ImpactPoint, HitResult.ImpactNormal, HitResult.TraceStart, RemovalSpec);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);
	return ExecuteRemoveRequest(Request, GetInteractableActorFromHit(HitResult));
}

float UDRSnowRemoveComponent::TryRemoveSnowAtLocation(
	FVector WorldLocation,
	FVector SurfaceNormal,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowAtLocation(WorldLocation, SurfaceNormal.GetSafeNormal(), RemovalSpec);
		return 0.f;
	}

	if (!CanRemoveNow(RemovalSpec))
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	const FVector BrushOrigin = IsValid(Owner) ? Owner->GetActorLocation() : WorldLocation;
	const FDRSnowSurfaceRemoveRequest Request =
		MakeRemoveRequest(WorldLocation, SurfaceNormal, BrushOrigin, RemovalSpec);

	return ExecuteRemoveRequest(Request);
}

float UDRSnowRemoveComponent::TryRemoveSnowAlongDirection(
	FVector BrushOrigin,
	FVector Direction,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !CanRemoveNow(RemovalSpec))
	{
		return 0.f;
	}

	const FVector NormalizedDirection = Direction.GetSafeNormal();
	if (NormalizedDirection.IsNearlyZero() || RemovalSpec.SnowAbsorbRange <= 0.f)
	{
		return 0.f;
	}

	LastRemoveTime = GetWorld()->GetTimeSeconds();

	const FVector FrustumOrigin = BrushOrigin + NormalizedDirection * RemovalSpec.SnowAbsorbStartOffset;
	const FVector FrustumEnd = FrustumOrigin + NormalizedDirection * RemovalSpec.SnowAbsorbRange;
	const FDRSnowSurfaceRemoveRequest Request = MakeRemoveRequest(
		FrustumEnd,
		-NormalizedDirection,
		FrustumOrigin,
		RemovalSpec);
	return ExecuteRemoveRequest(Request);
}

FDRSnowSurfaceRemoveRequest UDRSnowRemoveComponent::MakeRemoveRequest(
	FVector WorldLocation,
	FVector SurfaceNormal,
	FVector BrushOrigin,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	FDRSnowSurfaceRemoveRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	Request.BrushOrigin = BrushOrigin;
	Request.Radius = FMath::Max(0.f, RemovalSpec.SnowAbsorbRadius);
	Request.RequestedAmount = FMath::Max(0.f, RemovalSpec.SnowAbsorbPower);
	Request.RemovalBrushShape = RemovalSpec.RemovalBrushShape;
	Request.RemovalMode = RemovalSpec.RemovalMode;
	Request.AbsorbInnerRadiusRatio = FMath::Clamp(RemovalSpec.SnowAbsorbInnerRadiusRatio, 0.f, 1.f);
	Request.Context = MakeInteractionContext();
	return Request;
}

float UDRSnowRemoveComponent::ExecuteRemoveRequest(const FDRSnowSurfaceRemoveRequest& Request, AActor* FallbackTarget)
{
	float RemovedAmount = 0.f;
	if (UWorld* World = GetWorld())
	{
		if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
		{
			RemovedAmount = SnowSubsystem->RemoveSnow(Request).RemovedAmount;
			if (RemovedAmount > 0.f)
			{
				if (ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>())
				{
					FDRSnowRemoveOperation Operation;
					Operation.WorldLocation = Request.WorldLocation;
					Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
					Operation.BrushOrigin = Request.BrushOrigin;
					Operation.Radius = Request.Radius;
					Operation.RequestedAmount = Request.RequestedAmount;
					Operation.AppliedAmount = RemovedAmount;
					Operation.RemovalBrushShape = Request.RemovalBrushShape;
					Operation.RemovalMode = Request.RemovalMode;
					Operation.AbsorbInnerRadiusRatio = Request.AbsorbInnerRadiusRatio;
					Operation.TeamId = Request.Context.TeamId;
					Operation.VoxelWorldName =
						IsValid(Request.TargetVoxelWorld.Get())
							? Request.TargetVoxelWorld->GetFName()
							: NAME_None;
					MiningGameState->RegisterSnowRemove(Operation);
				}
			}
		}
	}

	if (RemovedAmount <= 0.f &&
		IsValid(FallbackTarget) &&
		FallbackTarget->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowRemove(FallbackTarget, Request))
	{
		RemovedAmount = IDRSnowInteractableInterface::Execute_ReceiveSnowRemoved(FallbackTarget, Request);
	}

	OnSnowRemoved.Broadcast(Request, RemovedAmount);
	return FMath::Max(0.f, RemovedAmount);
}

bool UDRSnowRemoveComponent::CanRemoveNow(const FDRSnowRemovalSpec& RemovalSpec) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const float AbsorbSpeed = FMath::Max(0.f, RemovalSpec.SnowAbsorbSpeed);
	if (AbsorbSpeed <= UE_SMALL_NUMBER)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastRemoveTime >= 1.f / AbsorbSpeed;
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowFromHit_Implementation(
	const FHitResult& HitResult,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	TryRemoveSnowFromHit(HitResult, RemovalSpec);
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowAtLocation_Implementation(
	FVector_NetQuantize WorldLocation,
	FVector_NetQuantizeNormal SurfaceNormal,
	const FDRSnowRemovalSpec& RemovalSpec)
{
	TryRemoveSnowAtLocation(WorldLocation, SurfaceNormal, RemovalSpec);
}

AVoxelWorld* UDRSnowRemoveComponent::GetVoxelWorldFromHit(const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent) ? Cast<AVoxelWorld>(HitComponent->GetOwner()) : nullptr;
}
