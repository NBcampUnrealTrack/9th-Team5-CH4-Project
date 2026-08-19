#include "DRSnowRemoveComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSurfaceSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRSnowVolumeSubsystem.h"
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
		if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
		{
			// Voxel 편집은 컴포넌트가 직접 수행하지 않고 Subsystem 경계로 보낸다.
			RemovedAmount = SnowSurfaceSubsystem->RemoveSnowAtArea(Request);
		}

		if (RemovedAmount > 0.f)
		{
			if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
			{
				if (UDRSnowVolumeSubsystem* SnowVolumeSubsystem =
					World->GetSubsystem<UDRSnowVolumeSubsystem>())
				{
					// 표면이 실제로 깎인 양만 원본 density에서도 제거한다.
					// Custom 계열 툴은 SurfaceSubsystem에서 실제 변화 voxel 기준으로 이미 처리한다.
					FDRSnowSurfaceRemoveRequest VolumeRequest = Request;
					VolumeRequest.RequestedAmount = RemovedAmount;
					SnowVolumeSubsystem->RemoveSnow(VolumeRequest);
				}
			}

			if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem =
				World->GetSubsystem<UDRSnowSurfaceSubsystem>())
			{
				// SnowVolume 감소 후 현재 표면을 다시 찾아 남은 dominant team 색으로 복원한다.
				SnowSurfaceSubsystem->RepaintSnowMaterialsAtArea(Request);
			}

			if (ADRMiningGameStateBase* MiningGameState =
				World->GetGameState<ADRMiningGameStateBase>())
			{
				FDRSnowRemoveOperation Operation;
				Operation.WorldLocation = Request.WorldLocation;
				Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
				Operation.Radius = Request.Radius;
				Operation.RequestedAmount = Request.RequestedAmount;
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
		if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
		{
			RemovedAmount = SnowSurfaceSubsystem->RemoveSnowAtArea(Request);
		}

		if (RemovedAmount > 0.f)
		{
			if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
			{
				if (UDRSnowVolumeSubsystem* SnowVolumeSubsystem =
					World->GetSubsystem<UDRSnowVolumeSubsystem>())
				{
					FDRSnowSurfaceRemoveRequest VolumeRequest = Request;
					VolumeRequest.RequestedAmount = RemovedAmount;
					SnowVolumeSubsystem->RemoveSnow(VolumeRequest);
				}
			}

			if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem =
				World->GetSubsystem<UDRSnowSurfaceSubsystem>())
			{
				SnowSurfaceSubsystem->RepaintSnowMaterialsAtArea(Request);
			}

			if (ADRMiningGameStateBase* MiningGameState =
				World->GetGameState<ADRMiningGameStateBase>())
			{
				FDRSnowRemoveOperation Operation;
				Operation.WorldLocation = Request.WorldLocation;
				Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
				Operation.Radius = Request.Radius;
				Operation.RequestedAmount = Request.RequestedAmount;
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
