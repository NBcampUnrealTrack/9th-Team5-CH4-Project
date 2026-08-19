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

void UDRSnowRemoveComponent::StartSnowRemoval()
{
}

void UDRSnowRemoveComponent::StopSnowRemoval()
{
}

void UDRSnowRemoveComponent::ApplyRemovalSettings(
	const FDRSnowRemovalSettings& NewSettings)
{
	RemovalSettings = NewSettings;
	RemovalSettings.AbsorbRange = FMath::Max(0.f, RemovalSettings.AbsorbRange);
	RemovalSettings.TraceSweepRadius = FMath::Max(0.f, RemovalSettings.TraceSweepRadius);
	RemovalSettings.AbsorbRadius = FMath::Max(0.f, RemovalSettings.AbsorbRadius);
	RemovalSettings.AbsorbStrength = FMath::Max(0.f, RemovalSettings.AbsorbStrength);
	RemovalSettings.AbsorbInterval = FMath::Max(0.f, RemovalSettings.AbsorbInterval);
}

float UDRSnowRemoveComponent::TryRemoveSnowFromHit(
	const FHitResult& HitResult)
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

	FDRSnowSurfaceRemoveRequest Request = MakeRemoveRequest(HitResult.ImpactPoint, HitResult.ImpactNormal);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);

	float RemovedAmount = 0.f;
	if (UWorld* World = GetWorld())
	{
		if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
		{
			RemovedAmount = SnowSubsystem->RemoveSnow(Request).RemovedAmount;

			if (RemovedAmount > 0.f)
			{
				if (ADRMiningGameStateBase* MiningGameState =
					World->GetGameState<ADRMiningGameStateBase>())
				{
					FDRSnowRemoveOperation Operation;
					Operation.WorldLocation = Request.WorldLocation;
					Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
					Operation.Radius = Request.Radius;
					Operation.RequestedAmount = Request.RequestedAmount;
					Operation.AppliedAmount = RemovedAmount;
					Operation.bInvertSurfaceStrength = Request.bInvertSurfaceStrength;
					Operation.EditTool = Request.EditTool;
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

	AActor* TargetActor = GetInteractableActorFromHit(HitResult);
	if (RemovedAmount <= 0.f &&
		IsValid(TargetActor) &&
		TargetActor->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowRemove(TargetActor, Request))
	{
		RemovedAmount = IDRSnowInteractableInterface::Execute_ReceiveSnowRemoved(TargetActor, Request);
	}

	OnSnowRemoved.Broadcast(Request, RemovedAmount);
	return FMath::Max(0.f, RemovedAmount);
}

float UDRSnowRemoveComponent::TryRemoveSnowAtLocation(
	FVector WorldLocation,
	FVector SurfaceNormal)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryRemoveSnowAtLocation(
			WorldLocation,
			SurfaceNormal.GetSafeNormal());
		return 0.f;
	}

	if (!CanRemoveNow())
	{
		return 0.f;
	}
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	const FDRSnowSurfaceRemoveRequest Request = MakeRemoveRequest(WorldLocation, SurfaceNormal);

	float RemovedAmount = 0.f;
	if (UWorld* World = GetWorld())
	{
		if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
		{
			RemovedAmount = SnowSubsystem->RemoveSnow(Request).RemovedAmount;

			if (RemovedAmount > 0.f)
			{
				if (ADRMiningGameStateBase* MiningGameState =
					World->GetGameState<ADRMiningGameStateBase>())
				{
					FDRSnowRemoveOperation Operation;
					Operation.WorldLocation = Request.WorldLocation;
					Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
					Operation.Radius = Request.Radius;
					Operation.RequestedAmount = Request.RequestedAmount;
					Operation.AppliedAmount = RemovedAmount;
					Operation.bInvertSurfaceStrength = Request.bInvertSurfaceStrength;
					Operation.EditTool = Request.EditTool;
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

	OnSnowRemoved.Broadcast(Request, RemovedAmount);
	return FMath::Max(0.f, RemovedAmount);
}

FDRSnowSurfaceRemoveRequest UDRSnowRemoveComponent::MakeRemoveRequest(
	FVector WorldLocation,
	FVector SurfaceNormal)
{
	FDRSnowSurfaceRemoveRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal =
		SurfaceNormal.IsNearlyZero()
			? FVector::UpVector
			: SurfaceNormal.GetSafeNormal();
	Request.Radius = RemovalSettings.AbsorbRadius;
	Request.RequestedAmount = RemovalSettings.AbsorbStrength;
	Request.bInvertSurfaceStrength = RemovalSettings.bInvertSurfaceStrength;
	Request.EditTool = RemovalSettings.EditTool;
	Request.Context = MakeInteractionContext();
	return Request;
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

void UDRSnowRemoveComponent::ServerStartSnowRemoval_Implementation()
{
	StartSnowRemoval();
}

void UDRSnowRemoveComponent::ServerStopSnowRemoval_Implementation()
{
	StopSnowRemoval();
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowFromHit_Implementation(
	const FHitResult& HitResult)
{
	TryRemoveSnowFromHit(HitResult);
}

void UDRSnowRemoveComponent::ServerTryRemoveSnowAtLocation_Implementation(
	FVector_NetQuantize WorldLocation,
	FVector_NetQuantizeNormal SurfaceNormal)
{
	TryRemoveSnowAtLocation(WorldLocation, SurfaceNormal);
}

AVoxelWorld* UDRSnowRemoveComponent::GetVoxelWorldFromHit(
	const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent) ? Cast<AVoxelWorld>(HitComponent->GetOwner()) : nullptr;
}
