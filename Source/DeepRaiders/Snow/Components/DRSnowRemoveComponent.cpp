#include "DRSnowRemoveComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "VoxelWorld.h"

UDRSnowRemoveComponent::UDRSnowRemoveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRSnowRemoveComponent::ApplyRemovalSettings(const FDRSnowRemovalSettings& NewSettings)
{
	RemovalSettings = NewSettings;
	RemovalSettings.AbsorbRadius = FMath::Max(0.f, RemovalSettings.AbsorbRadius);
	RemovalSettings.AbsorbStrength = FMath::Max(0.f, RemovalSettings.AbsorbStrength);
	RemovalSettings.AbsorbInterval = FMath::Max(0.f, RemovalSettings.AbsorbInterval);
}

float UDRSnowRemoveComponent::TryRemoveSnowFromHit(const FHitResult& HitResult)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowFromHit(HitResult);
		return 0.f;
	}

	if (!HitResult.bBlockingHit)
	{
		return 0.f;
	}

	if (!CanRemoveNow())
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	FDRSnowSurfaceRemoveRequest Request =
		MakeRemoveRequest(HitResult.ImpactPoint, HitResult.ImpactNormal,HitResult.TraceStart);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);
	return ExecuteRemoveRequest(Request, GetInteractableActorFromHit(HitResult));
}

float UDRSnowRemoveComponent::TryRemoveSnowAtLocation(FVector WorldLocation, FVector SurfaceNormal)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowAtLocation(WorldLocation, SurfaceNormal.GetSafeNormal());
		return 0.f;
	}

	if (!CanRemoveNow())
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	const FVector BrushOrigin = IsValid(Owner) ? Owner->GetActorLocation() : WorldLocation;
	const FDRSnowSurfaceRemoveRequest Request = MakeRemoveRequest(WorldLocation, SurfaceNormal, BrushOrigin);

	return ExecuteRemoveRequest(Request);
}

FDRSnowSurfaceRemoveRequest UDRSnowRemoveComponent::MakeRemoveRequest(
	FVector WorldLocation,
	FVector SurfaceNormal,
	FVector BrushOrigin)
{
	FDRSnowSurfaceRemoveRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	Request.BrushOrigin = BrushOrigin;
	Request.Radius = RemovalSettings.AbsorbRadius;
	Request.RequestedAmount = RemovalSettings.AbsorbStrength;
	Request.RemovalBrushShape = RemovalSettings.RemovalBrushShape;
	Request.RemovalMode = RemovalSettings.RemovalMode;
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

bool UDRSnowRemoveComponent::CanRemoveNow() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	return World->GetTimeSeconds() - LastRemoveTime >= RemovalSettings.AbsorbInterval;
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowFromHit_Implementation(const FHitResult& HitResult)
{
	TryRemoveSnowFromHit(HitResult);
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowAtLocation_Implementation(
	FVector_NetQuantize WorldLocation,
	FVector_NetQuantizeNormal SurfaceNormal)
{
	TryRemoveSnowAtLocation(WorldLocation, SurfaceNormal);
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
