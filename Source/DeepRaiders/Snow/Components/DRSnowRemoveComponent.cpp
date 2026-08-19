#include "DRSnowRemoveComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSurfaceSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRSnowVolumeSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "VoxelWorld.h"

UDRSnowRemoveComponent::UDRSnowRemoveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDRSnowRemoveComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRemovingSnow || !CanRemoveNow())
	{
		return;
	}

	// 아무 표면을 맞추지 못해도 시도 간격은 소비한다.
	// 실패 시 매 프레임 trace하는 상황을 막기 위한 처리다.
	LastRemoveTime = GetWorld()->GetTimeSeconds();

	FHitResult HitResult;
	if (PerformRemovalTrace(HitResult))
	{
		TryRemoveSnowFromHit(HitResult);
	}
}

void UDRSnowRemoveComponent::StartSnowRemoval()
{
	bRemovingSnow = true;
	SetComponentTickEnabled(true);
}

void UDRSnowRemoveComponent::StopSnowRemoval()
{
	bRemovingSnow = false;
	SetComponentTickEnabled(false);
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
	if (!HitResult.bBlockingHit)
	{
		return 0.f;
	}

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

bool UDRSnowRemoveComponent::PerformRemovalTrace(FHitResult& OutHitResult) const
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!IsValid(World) || !IsValid(Owner))
	{
		return false;
	}

	FVector TraceStart = Owner->GetActorLocation();
	FRotator TraceRotation = Owner->GetActorRotation();

	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!IsValid(OwnerPawn))
	{
		OwnerPawn = Owner->GetInstigator();
	}

	if (IsValid(OwnerPawn))
	{
		if (AController* Controller = OwnerPawn->GetController())
		{
			// 플레이어 장비에 붙은 경우 캐릭터 forward보다 실제 조준 시점을 우선한다.
			Controller->GetPlayerViewPoint(TraceStart, TraceRotation);
		}
		else
		{
			OwnerPawn->GetActorEyesViewPoint(TraceStart, TraceRotation);
		}
	}

	const FVector TraceEnd = TraceStart + (TraceRotation.Vector() * RemovalSettings.AbsorbRange);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SnowRemovalTrace), false);
	QueryParams.bTraceComplex = bTraceComplex;
	QueryParams.AddIgnoredActor(Owner);
	if (IsValid(OwnerPawn))
	{
		QueryParams.AddIgnoredActor(OwnerPawn);
	}

	switch (RemovalSettings.TraceMode)
	{
	case EDRSnowRemovalTraceMode::LineTrace:
		return World->LineTraceSingleByChannel(
			OutHitResult,
			TraceStart,
			TraceEnd,
			TraceChannel,
			QueryParams);

	case EDRSnowRemovalTraceMode::SphereSweep:
		return World->SweepSingleByChannel(
			OutHitResult,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(RemovalSettings.TraceSweepRadius),
			QueryParams);

	default:
		return false;
	}
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
