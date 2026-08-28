#include "DRSnowAddComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "EngineUtils.h"
#include "VoxelWorld.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogDRSnowAdd, Log, All);
}

bool UDRSnowAddComponent::TryAddSnowFromHit(
	const FHitResult& HitResult)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryAddSnowFromHit(HitResult);
		return true;
	}

	if (!HitResult.bBlockingHit)
	{
		UE_LOG(LogDRSnowAdd, Warning, TEXT("Rejected non-blocking hit: Owner=%s TraceStart=%s TraceEnd=%s"),
			*GetNameSafe(Owner), *HitResult.TraceStart.ToString(), *HitResult.TraceEnd.ToString());
		return false;
	}

	FDRSnowSurfaceAddRequest Request = MakeAddRequest(HitResult.ImpactPoint, HitResult.ImpactNormal);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);
	const bool bHitVoxelWorld = IsValid(Request.TargetVoxelWorld.Get());
	if (!IsValid(Request.TargetVoxelWorld.Get()))
	{
		Request.TargetVoxelWorld = ResolveFallbackVoxelWorld();
		Request.bUseVirtualSurface = Request.bAllowVirtualSurfaceFallback;
	}
	UE_LOG(LogDRSnowAdd, Log, TEXT("Hit: Actor=%s Component=%s Point=%s Normal=%s HitVoxel=%d TargetVoxel=%s Virtual=%d Fallback=%d Tool=%d Radius=%.1f Amount=%.3f"),
		*GetNameSafe(HitResult.GetActor()), *GetNameSafe(HitResult.GetComponent()),
		*HitResult.ImpactPoint.ToString(), *HitResult.ImpactNormal.ToString(), bHitVoxelWorld,
		*GetNameSafe(Request.TargetVoxelWorld.Get()), Request.bUseVirtualSurface,
		Request.bAllowVirtualSurfaceFallback, static_cast<int32>(Request.EditTool), Request.Radius, Request.Amount);

	return ExecuteAddRequest(Request, GetInteractableActorFromHit(HitResult));
}

bool UDRSnowAddComponent::TryAddSnowAtLocation(FVector WorldLocation, FVector SurfaceNormal)
{
	return TryAddSnow(MakeAddRequest(WorldLocation, SurfaceNormal));
}

bool UDRSnowAddComponent::TryAddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryAddSnow(Request);
		return true;
	}

	return ExecuteAddRequest(Request);
}

void UDRSnowAddComponent::SetAddSettings(float InAddRadius, float InAddAmount)
{
	AddRadius = FMath::Max(0.f, InAddRadius); 
	AddAmount = FMath::Max(0.f, InAddAmount);
}
void UDRSnowAddComponent::SetAddEditTool(EDRSnowVoxelEditTool InEditTool)
{
	AddEditTool = InEditTool;
}

void UDRSnowAddComponent::SetAllowVirtualSurfaceFallback(bool bInAllowVirtualSurfaceFallback)
{
	bAllowVirtualSurfaceFallback = bInAllowVirtualSurfaceFallback;
}

bool UDRSnowAddComponent::ExecuteAddRequest(
	const FDRSnowSurfaceAddRequest& Request,
	AActor* FallbackTarget)
{
	bool bHandled = false;
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
		{
			bHandled = SnowSubsystem->AddSnow(Request).AddedAmount > 0.f;
			UE_LOG(LogDRSnowAdd, Log, TEXT("Subsystem result: Handled=%d TargetVoxel=%s Location=%s"),
				bHandled, *GetNameSafe(Request.TargetVoxelWorld.Get()), *Request.WorldLocation.ToString());
		}

		if (bHandled)
		{
			if (ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>())
			{
				FDRSnowAddOperation Operation;
				Operation.WorldLocation = Request.WorldLocation;
				Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
				Operation.ImpactDirection = Request.ImpactDirection.GetSafeNormal();
				Operation.Radius = Request.Radius;
				Operation.Amount = Request.Amount;
				Operation.EditTool = Request.EditTool;
				Operation.bAllowVirtualSurfaceFallback = Request.bAllowVirtualSurfaceFallback;
				Operation.bUseVirtualSurface = Request.bUseVirtualSurface;
				Operation.TeamId = Request.Context.TeamId;
				Operation.VoxelWorldName = IsValid(Request.TargetVoxelWorld.Get())
					? Request.TargetVoxelWorld->GetFName()
					: NAME_None;
				MiningGameState->RegisterSnowAdd(Operation);
			}
		}
	}

	if (!bHandled &&
		IsValid(FallbackTarget) &&
		FallbackTarget->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowAdd(FallbackTarget, Request))
	{
		IDRSnowInteractableInterface::Execute_ReceiveSnowAdded(FallbackTarget, Request);
		bHandled = true;
	}

	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

void UDRSnowAddComponent::ServerTryAddSnowFromHit_Implementation(const FHitResult& HitResult)
{
	TryAddSnowFromHit(HitResult);
}

void UDRSnowAddComponent::ServerTryAddSnow_Implementation(const FDRSnowSurfaceAddRequest& Request)
{
	ExecuteAddRequest(Request);
}

FDRSnowSurfaceAddRequest UDRSnowAddComponent::MakeAddRequest(FVector WorldLocation, FVector SurfaceNormal)
{
	FDRSnowSurfaceAddRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	Request.ImpactDirection = -Request.SurfaceNormal;
	Request.Radius = AddRadius;
	Request.Amount = AddAmount;
	Request.EditTool = AddEditTool;
	Request.bAllowVirtualSurfaceFallback = bAllowVirtualSurfaceFallback;
	Request.Context = MakeInteractionContext();
	return Request;
}

AVoxelWorld* UDRSnowAddComponent::GetVoxelWorldFromHit(const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent) ? Cast<AVoxelWorld>(HitComponent->GetOwner()) : nullptr;
}

AVoxelWorld* UDRSnowAddComponent::ResolveFallbackVoxelWorld() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}
